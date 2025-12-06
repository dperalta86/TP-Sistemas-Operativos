#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include <connection/serialization.h>
#include <connection/protocol.h>
#include <operations/read_block.h> // Asumo que tus funciones están aquí
#include <config/storage_config.h>
#include <globals/globals.h>
#include <fresh_start/fresh_start.h>
#include <string.h>
#include "test_utils.h" // Funciones auxiliares de test
#include "errors.h"
#include <cspecs/cspec.h>

// --- Definiciones y Helpers asumidos para el test ---

// Helper para crear un archivo de bloque con contenido específico
void create_test_block_file(const char *path, const char *content, size_t size) {
    FILE *file = fopen(path, "wb");
    if (file) {
        // Escribe solo 'size' bytes para simular el contenido del bloque.
        fwrite(content, 1, size, file); 
        fclose(file);
    }
}

// --- Inicio del Contexto de Pruebas ---

context(tests_read_block) {
    
    // =========================================================================
    // 1. Tests para deserialize_block_read_request
    // =========================================================================
    describe("Deserialización de solicitud READ BLOCK") {
        before {
            g_storage_logger = create_test_logger();
        } end

        after {
            destroy_test_logger(g_storage_logger);
        } end

        it("Deserializa correctamente todos los datos de una solicitud valida") {
            uint32_t query_id;
            char *name = NULL;
            char *tag = NULL;
            uint32_t block_number;

            t_package *package = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);

            package_add_uint32(package, 42);        // query_id
            package_add_string(package, "archivo");  // name
            package_add_string(package, "tagX");     // tag
            package_add_uint32(package, 15);        // block_number

            package_simulate_reception(package);

            int retval = deserialize_block_read_request(package, &query_id, &name, &tag, &block_number);
            
            should_int(retval) be equal to (0);
            should_int(query_id) be equal to (42);
            should_string(name) be equal to ("archivo");
            should_string(tag) be equal to ("tagX");
            should_int(block_number) be equal to (15);

            package_destroy(package);
            free(name);
            free(tag);
        } end

        it("Falla al deserializar el tag y limpia el nombre") {
            uint32_t query_id;
            char *name = NULL;
            char *tag = NULL;
            uint32_t block_number;

            t_package *package = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);

            package_add_uint32(package, 43);
            package_add_string(package, "archivo_limpio");
            // Se asume que package_read_string fallará aquí o la cadena para tag no se añade

            package_simulate_reception(package);

            // Simulación de paquete incompleto para forzar fallo en el tag
            int retval = deserialize_block_read_request(package, &query_id, &name, &tag, &block_number);
            
            should_int(retval) be equal to (-1);
            should_ptr(tag) be null;
            should_ptr(name) be null;

            package_destroy(package);
            if (name) free(name); // Liberación final si el test de limpieza fuera defectuoso
        } end
    } end

    // =========================================================================
    // 2. Tests para read_from_logical_block (Lectura física)
    // =========================================================================
    describe("Lectura física de bloque lógico") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            // Tamaño de bloque pequeño para facilidad de test: 10 bytes
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 100, 10, "INFO"); 
            create_test_superblock(TEST_MOUNT_POINT);

            char config_path[PATH_MAX];
            snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
            g_storage_config = create_storage_config(config_path);

            // Crea el directorio para el archivo: TEST_MOUNT_POINT/files/file1/tag1/logical_blocks/
            init_logical_blocks("file1", "tag1", 20, TEST_MOUNT_POINT); 
        } end

        after {
            destroy_storage_config(g_storage_config);
            cleanup_test_directory();
            destroy_test_logger(g_storage_logger);
        } end

        it("Lee el bloque exitosamente y lo null-termina") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path),
                     "%s/files/file1/tag1/logical_blocks/%04d.dat", TEST_MOUNT_POINT, 5);
            
            // Contenido de 10 bytes
            char *content = "0123456789"; 
            create_test_block_file(logical_block_path, content, g_storage_config->block_size);

            // Asignar el buffer con BLOCK_SIZE + 1 para el '\0'
            void *read_buffer = malloc(g_storage_config->block_size + 1); 

            int retval = read_from_logical_block(100, "file1", "tag1", 5, read_buffer);

            should_int(retval) be equal to (0);
            should_string(read_buffer) be equal to (content); // String chequea hasta el '\0'
            
            // Chequeamos explícitamente el null terminator
            should_char(((char*)read_buffer)[g_storage_config->block_size]) be equal to ('\0'); 
            
            free(read_buffer);
        } end
        
        it("Falla al abrir el archivo de bloque (-1)") {
            // Intentar leer un bloque inexistente (ej. bloque 9999)
            void *read_buffer = malloc(g_storage_config->block_size + 1); 
            
            int retval = read_from_logical_block(101, "file1", "tag1", 9999, read_buffer);

            should_int(retval) be equal to (-1); // Fallo al abrir (fopen == NULL)

            free(read_buffer);
        } end

        it("Falla la lectura: EOF prematuro o Lectura parcial (-3)") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path),
                     "%s/files/file1/tag1/logical_blocks/%04d.dat", TEST_MOUNT_POINT, 6);
            
            // Contenido de 5 bytes, menor que BLOCK_SIZE (10 bytes)
            const char *content = "ABCDE"; 
            create_test_block_file(logical_block_path, content, 5);

            void *read_buffer = malloc(g_storage_config->block_size + 1); 

            int retval = read_from_logical_block(102, "file1", "tag1", 6, read_buffer);

            should_int(retval) be equal to (-3); // Lectura parcial/EOF inesperado

            free(read_buffer);
        } end

        // Nota: Simular ferror() o fclose() fallido requiere inyeccion de mocks 
        // o un entorno más complejo, por lo que nos enfocaremos en los errores de I/O de fopen/fread.
    } end

    // =========================================================================
    // 3. Tests para execute_block_read (Lógica central)
    // =========================================================================
    describe ("Lógica central de lectura de bloques") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 1000, 10, "INFO");
            create_test_superblock(TEST_MOUNT_POINT);

            char config_path[PATH_MAX];
            snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
            g_storage_config = create_storage_config(config_path);

            g_open_files_dict = dictionary_create();
            init_physical_blocks(TEST_MOUNT_POINT, g_storage_config->fs_size, g_storage_config->block_size);
        } end

        after {
            cleanup_file_sync();
            destroy_storage_config(g_storage_config);
            cleanup_test_directory();
            destroy_test_logger(g_storage_logger);
        } end

        it ("El file:tag no existe (-10)") {
            void *read_buffer = malloc(g_storage_config->block_size + 1);
            int retval = execute_block_read("file1", "tag1", 12, 3, read_buffer);

            should_int(retval) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
            free(read_buffer);
        } end
        
        it ("Bloque lógico fuera de rango (-11)") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            // Crea metadata: 3 bloques (indices 0, 1, 2)
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "COMMITTED", TEST_MOUNT_POINT); 
            
            void *read_buffer = malloc(g_storage_config->block_size + 1);
            int retval = execute_block_read("file1", "tag1", 12, 7, read_buffer); // Bloque 7 (max es 2)

            should_int(retval) be equal to (READ_OUT_OF_BOUNDS);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
            free(read_buffer);
        } end

        it ("Lectura exitosa del bloque") {
            // Setup para el éxito:
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "COMMITTED", TEST_MOUNT_POINT);
            
            // Escribir contenido en el archivo de bloque (simulando que existe)
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path),
                     "%s/files/file1/tag1/logical_blocks/%04d.dat", TEST_MOUNT_POINT, 2);
            char *content = "TEST_DATA_"; // 10 bytes
            create_test_block_file(logical_block_path, content, g_storage_config->block_size);
            
            void *read_buffer = malloc(g_storage_config->block_size + 1);
            
            int retval = execute_block_read("file1", "tag1", 12, 2, read_buffer);

            should_int(retval) be equal to (0);
            should_string(read_buffer) be equal to (content);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
            free(read_buffer);
        } end
    } end


    // =========================================================================
    // 4. Tests para handle_read_block_request (Manejador completo)
    // =========================================================================
    describe ("Manejador de solicitud READ BLOCK (De punta a punta)") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 1000, 10, "INFO"); // 10 bytes de bloque
            create_test_superblock(TEST_MOUNT_POINT);

            char config_path[PATH_MAX];
            snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
            g_storage_config = create_storage_config(config_path);

            g_open_files_dict = dictionary_create();
            init_physical_blocks(TEST_MOUNT_POINT, g_storage_config->fs_size, g_storage_config->block_size);
        } end

        after {
            cleanup_file_sync();
            destroy_storage_config(g_storage_config);
            cleanup_test_directory();
            destroy_test_logger(g_storage_logger);
        } end

        it ("Manejo de lectura exitosa y paquete de respuesta con datos") {
            // Setup para el éxito:
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "COMMITTED", TEST_MOUNT_POINT);
            
            // Contenido a leer
            char *content = "READ_OK_12"; // 10 bytes
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path),
                     "%s/files/file1/tag1/logical_blocks/%04d.dat", TEST_MOUNT_POINT, 1);
            create_test_block_file(logical_block_path, content, g_storage_config->block_size);

            // Crear paquete de solicitud
            t_package *request_package = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);
            package_add_uint32(request_package, (uint32_t)12); // query_id
            package_add_string(request_package, "file1");
            package_add_string(request_package, "tag1");
            package_add_uint32(request_package, (uint32_t)1); // block_number
            package_simulate_reception(request_package);

            t_package *response = handle_read_block_request(request_package);

            should_ptr(response) not be null;
            should_int(response->operation_code) be equal to (STORAGE_OP_BLOCK_READ_RES);
            
            // 1. Leer status (int8_t)
            int8_t success_code;
            package_read_int8(response, &success_code);
            should_int(success_code) be equal to (0);

            // 2. Leer contenido del bloque (string, longitud 10)
            char *read_content = package_read_string(response);
            should_string(read_content) be equal to (content);
            
            should_bool(correct_unlock("file1", "tag1")) be truthy;
            
            free(read_content);
            package_destroy(response);
            package_destroy(request_package);
        } end

        it ("El manejador falla en la ejecucion y devuelve un paquete con código de error") {
            // NO creamos el directorio 'file1:tag1' para forzar FILE_TAG_MISSING
            
            t_package *request_package = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);
            package_add_uint32(request_package, (uint32_t)13);
            package_add_string(request_package, "file_missing");
            package_add_string(request_package, "tag_missing");
            package_add_uint32(request_package, (uint32_t)3);
            package_simulate_reception(request_package);

            t_package *response = handle_read_block_request(request_package);

            should_ptr(response) not be null;
            should_int(response->operation_code) be equal to (STORAGE_OP_BLOCK_READ_RES);
            
            // Leer status (int8_t)
            int8_t err_code;
            package_read_int8(response, &err_code);
            should_int(err_code) be equal to (FILE_TAG_MISSING);
            
            // Verificamos que no se haya añadido contenido (el offset solo avanzó 1 byte por el status)
            should_int(response->buffer->offset) be equal to (sizeof(int8_t));

            should_bool(correct_unlock("file_missing", "tag_missing")) be truthy;
            
            package_destroy(response);
            package_destroy(request_package);
        } end

        it("Falla la deserializacion y devuelve NULL") {
            // Paquete incompleto que falla al leer el tag (por ejemplo)
            t_package *request_package = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);
            package_add_uint32(request_package, (uint32_t)14);
            package_add_string(request_package, "file_ok");
            package_simulate_reception(request_package);
            // No se añade el tag ni el block_number, forzando fallo en deserialize

            t_package *response = handle_read_block_request(request_package);

            should_ptr(response) be null;

            // En este caso, no se hace un lock/unlock, por lo que no es necesario chequear 'correct_unlock'.
            package_destroy(request_package);
        } end
    } end
}