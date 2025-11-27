#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h> 
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <globals/globals.h>
#include <fresh_start/fresh_start.h>
#include "config/storage_config.h"
#include "connection/protocol.h" 
#include "connection/serialization.h"
#include "errors.h"
#include "test_utils.h"
#include <operations/commit_tag.h>
#include <cspecs/cspec.h>

void setup_filesystem_environment() {
    create_test_directory();
    create_test_superblock(TEST_MOUNT_POINT);
    create_test_blocks_hash_index(TEST_MOUNT_POINT);
    init_bitmap(TEST_MOUNT_POINT, TEST_FS_SIZE, TEST_BLOCK_SIZE);
    create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 100, 10, "INFO"); 

    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
    g_storage_config = create_storage_config(config_path);

    g_open_files_dict = dictionary_create();
    init_physical_blocks(TEST_MOUNT_POINT, g_storage_config->fs_size, g_storage_config->block_size);
}

void teardown_filesystem_environment() {
    destroy_storage_config(g_storage_config);
    cleanup_file_sync();
    cleanup_test_directory();
}

context(tests_commit_tag) {
    
    // -------------------------------------------------------------------------
    // 1. Tests para deserialize_tag_commit_request
    // -------------------------------------------------------------------------
    describe("Deserialización de solicitud COMMIT TAG") {
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

            t_package *package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);

            package_add_uint32(package, 100);        
            package_add_string(package, "config_file");  
            package_add_string(package, "v1");     

            package_simulate_reception(package);

            int retval = deserialize_tag_commit_request(package, &query_id, &name, &tag);
            
            should_int(retval) be equal to (0);
            should_int(query_id) be equal to (100);
            should_string(name) be equal to ("config_file");
            should_string(tag) be equal to ("v1");

            package_destroy(package);
            free(name);
            free(tag);
        } end

        it("Falla al deserializar el tag y limpia la memoria asignada para el nombre") {
            uint32_t query_id = 0;
            char *name = NULL;
            char *tag = NULL;

            t_package *package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);

            package_add_uint32(package, 101);
            package_add_string(package, "file_to_clean");

            package_simulate_reception(package);
            
            int retval = deserialize_tag_commit_request(package, &query_id, &name, &tag);
            
            should_int(retval) be equal to (-3);
            should_ptr(tag) be null;
            should_ptr(name) be null; 

            package_destroy(package);
            if (name) free(name);
        } end
    } end

    // -------------------------------------------------------------------------
    // 2. Tests para execute_tag_commit (Lógica central)
    // -------------------------------------------------------------------------
    describe ("Lógica central de commit de tag") {
        before {
            g_storage_logger = create_test_logger();
            setup_filesystem_environment();
        } end

        after {
            teardown_filesystem_environment();
            destroy_test_logger(g_storage_logger);
        } end

        it ("Debe comitear exitosamente un archivo en estado WORK_IN_PROGRESS y registrar bloques unicos") {
            char *name = "file1";
            char *tag = "tag1";
            char *content = "Este es un contenido unico para el commit.";
            
            init_logical_blocks(name, tag, 1, TEST_MOUNT_POINT);
            write_physical_block_content(3, content, strlen(content));
            link_logical_to_physical(name, tag, 0, 3);
            create_test_metadata(name, tag, 1, "[3]", (char*)IN_PROGRESS, g_storage_config->mount_point);

            int result = execute_tag_commit(200, name, tag);

            should_int(result) be equal to (0);
            
            // verificación del registro de hash
            char hash_index_path[PATH_MAX];
            get_hash_index_config_path(hash_index_path);
            t_config *hash_config = config_create(hash_index_path);

            char *read_buffer = (char *)malloc(g_storage_config->block_size);
            memset(read_buffer, 0, g_storage_config->block_size);
            memcpy(read_buffer, content, strlen(content));

            char *hash = crypto_md5(read_buffer, g_storage_config->block_size);
            should_bool(config_has_property(hash_config, hash)) be truthy;
            
            char *ph_block_name = config_get_string_value(hash_config, hash);
            should_string(ph_block_name) be equal to ("block0003");
            
            t_file_metadata *metadata_after_commit = read_file_metadata(g_storage_config->mount_point, name, tag);
            should_string(metadata_after_commit->state) be equal to ((char*)COMMITTED);
            
            // Cleanup de verificación
            free(hash);
            free(read_buffer);
            config_destroy(hash_config);
            if (metadata_after_commit) destroy_file_metadata(metadata_after_commit);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end
        
        it ("Debe comitear exitosamente un archivo duplicado (liberando el bloque fisico anterior)") {
            char *name = "file1";
            char *tag1 = "tag1";
            char *tag2 = "tag2";
            char *content = "Contenido duplicado para commit final";

            // --- PASO 1: Setup archivo base (Simular commit previo) ---
            init_logical_blocks(name, tag1, 1, TEST_MOUNT_POINT);
            write_physical_block_content(3, content, strlen(content));
            link_logical_to_physical(name, tag1, 0, 3);
            create_test_metadata(name, tag1, 1, "[3]", (char*)IN_PROGRESS, g_storage_config->mount_point);
            define_bitmap_bit(3, true); // Simular que el bloque 3 ya está en uso

            execute_tag_commit(200, name, tag1);

            // --- PASO 2: Setup del archivo a commitear (file1:v2) ---
            init_logical_blocks(name, tag2, 1, TEST_MOUNT_POINT);
            write_physical_block_content(5, content, strlen(content));
            link_logical_to_physical(name, tag2, 0, 5);
            create_test_metadata(name, tag2, 1, "[5]", (char*)IN_PROGRESS, g_storage_config->mount_point);
            define_bitmap_bit(5, true); // Simular que el bloque 5 ya está en uso

            // Ejecución
            int result = execute_tag_commit(201, name, tag2);

            should_int(result) be equal to (0);
                        
            // verificación del registro de hash
            char hash_index_path[PATH_MAX];
            get_hash_index_config_path(hash_index_path);
            t_config *hash_config = config_create(hash_index_path);

            char *read_buffer = (char *)malloc(g_storage_config->block_size);
            memset(read_buffer, 0, g_storage_config->block_size);
            memcpy(read_buffer, content, strlen(content));

            char *hash = crypto_md5(read_buffer, g_storage_config->block_size);
            should_bool(config_has_property(hash_config, hash)) be truthy;
            
            char *ph_block_name = config_get_string_value(hash_config, hash);
            should_string(ph_block_name) be equal to ("block0003");            

            should_int(config_keys_amount(hash_config)) be equal to (1); // Solo un bloque físico debe estar registrado
            
            t_file_metadata *metadata_after_commit = read_file_metadata(g_storage_config->mount_point, name, tag2);
            should_string(metadata_after_commit->state) be equal to ((char*)COMMITTED);
            should_int(metadata_after_commit->blocks[0]) be equal to (3);

            t_bitarray *bitmap = NULL;
            char *bitmap_buffer = NULL;
            bitmap_load(&bitmap, &bitmap_buffer);
            should_bool(bitarray_test_bit(bitmap, 3)) be equal to (true); // block0003 debe estar en uso
            should_bool(bitarray_test_bit(bitmap, 5)) be equal to (false); // block0005 debe estar libre
            bitmap_close(bitmap, bitmap_buffer);
            
            // Cleanup de verificación
            free(hash);
            free(read_buffer);
            config_destroy(hash_config);
            if (metadata_after_commit) destroy_file_metadata(metadata_after_commit);
            should_bool(correct_unlock(name, tag1)) be truthy;
            should_bool(correct_unlock(name, tag2)) be truthy;
        } end

        it ("Debe comitear exitosamente un archivo con bloques duplicados (liberando los bloques fisicos anteriores)") {
            char *name = "file1";
            char *tag1 = "tag1";
            char *tag2 = "tag2";
            char *content = "Contenido duplicado para commit final";

            // --- PASO 1: Setup archivo base (Simular commit previo) ---
            init_logical_blocks(name, tag1, 1, TEST_MOUNT_POINT);
            write_physical_block_content(3, content, strlen(content));
            link_logical_to_physical(name, tag1, 0, 3);
            create_test_metadata(name, tag1, 1, "[3]", (char*)IN_PROGRESS, g_storage_config->mount_point);
            define_bitmap_bit(3, true); // Simular que el bloque 3 ya está en uso

            execute_tag_commit(200, name, tag1);

            // --- PASO 2: Setup del archivo a commitear (file1:v2) ---
            init_logical_blocks(name, tag2, 2, TEST_MOUNT_POINT);
            write_physical_block_content(5, content, strlen(content));
            write_physical_block_content(7, content, strlen(content));
            link_logical_to_physical(name, tag2, 0, 5);
            link_logical_to_physical(name, tag2, 1, 7);
            create_test_metadata(name, tag2, 2, "[5, 7]", (char*)IN_PROGRESS, g_storage_config->mount_point);
            define_bitmap_bit(5, true); // Simular que el bloque 5 ya está en uso
            define_bitmap_bit(7, true); // Simular que el bloque 5 ya está en uso

            // Ejecución
            int result = execute_tag_commit(201, name, tag2);

            should_int(result) be equal to (0);
                        
            // verificación del registro de hash
            char hash_index_path[PATH_MAX];
            get_hash_index_config_path(hash_index_path);
            t_config *hash_config = config_create(hash_index_path);

            char *read_buffer = (char *)malloc(g_storage_config->block_size);
            memset(read_buffer, 0, g_storage_config->block_size);
            memcpy(read_buffer, content, strlen(content));

            char *hash = crypto_md5(read_buffer, g_storage_config->block_size);
            should_bool(config_has_property(hash_config, hash)) be truthy;
            
            char *ph_block_name = config_get_string_value(hash_config, hash);
            should_string(ph_block_name) be equal to ("block0003");            

            should_int(config_keys_amount(hash_config)) be equal to (1); // Solo un bloque físico debe estar registrado
            
            t_file_metadata *metadata_after_commit = read_file_metadata(g_storage_config->mount_point, name, tag2);
            should_string(metadata_after_commit->state) be equal to ((char*)COMMITTED);
            should_int(metadata_after_commit->blocks[0]) be equal to (3);
            should_int(metadata_after_commit->blocks[1]) be equal to (3);

            t_bitarray *bitmap = NULL;
            char *bitmap_buffer = NULL;
            bitmap_load(&bitmap, &bitmap_buffer);
            should_bool(bitarray_test_bit(bitmap, 3)) be equal to (true); // block0003 debe estar en uso
            should_bool(bitarray_test_bit(bitmap, 5)) be equal to (false); // block0005 debe estar libre
            should_bool(bitarray_test_bit(bitmap, 7)) be equal to (false); // block0007 debe estar libre
            bitmap_close(bitmap, bitmap_buffer);
            
            // Cleanup de verificación
            free(hash);
            free(read_buffer);
            config_destroy(hash_config);
            if (metadata_after_commit) destroy_file_metadata(metadata_after_commit);
            should_bool(correct_unlock(name, tag1)) be truthy;
            should_bool(correct_unlock(name, tag2)) be truthy;
        } end

        it ("Debe retornar SUCCESS si el archivo ya esta en estado COMMITTED (Idempotencia)") {
            char *name = "file1";
            char *tag = "tag1";
            init_logical_blocks(name, tag, 1, TEST_MOUNT_POINT); 
            create_test_metadata(name, tag, 1, "[10]", (char*)COMMITTED, g_storage_config->mount_point);
            
            int result = execute_tag_commit(201, name, tag);

            should_int(result) be equal to (0);
            
            // VERIFICACIÓN: El estado debe seguir siendo COMMITTED
            t_file_metadata *metadata_after = read_file_metadata(g_storage_config->mount_point, name, tag);
            should_ptr(metadata_after) not be null;
            should_string(metadata_after->state) be equal to ((char*)COMMITTED);
            
            // Cleanup de verificación
            if (metadata_after) destroy_file_metadata(metadata_after);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

        it ("Debe retornar FILE_TAG_MISSING si el directorio del archivo no existe") {
            char *name = "file_missing";
            char *tag = "tag_missing";
            
            int result = execute_tag_commit(202, name, tag);

            should_int(result) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

        it ("Debe retornar FILE_TAG_MISSING si no se puede leer el metadata.config (read_file_metadata falla)") {
            char *name = "file_no_metadata";
            char *tag = "tag_no_metadata";
            init_logical_blocks(name, tag, 1, TEST_MOUNT_POINT); 
            
            int result = execute_tag_commit(203, name, tag);

            should_int(result) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

    } end


    // -------------------------------------------------------------------------
    // 3. Tests para handle_tag_commit_request (Manejador completo)
    // -------------------------------------------------------------------------
    describe ("Manejador de solicitud COMMIT TAG (De punta a punta)") {
        before {
            g_storage_logger = create_test_logger();
            setup_filesystem_environment();
            
            init_logical_blocks("file1", "tag1", 2, TEST_MOUNT_POINT); 
            create_test_metadata("file1", "tag1", 1, "[1]", (char*)IN_PROGRESS, g_storage_config->mount_point);
        } end

        after {
            teardown_filesystem_environment();
            destroy_test_logger(g_storage_logger);
        } end

        it ("Manejo de commit exitoso y paquete de respuesta con SUCCESS code") {
            char *name = "file1";
            char *tag = "tag1";

            t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
            package_add_uint32(request_package, (uint32_t)300);
            package_add_string(request_package, name);
            package_add_string(request_package, tag);
            package_simulate_reception(request_package);

            t_package *response = handle_tag_commit_request(request_package);

            should_ptr(response) not be null;
            should_int(response->operation_code) be equal to (STORAGE_OP_TAG_COMMIT_RES);
            
            int8_t status_code;
            package_read_int8(response, &status_code);
            should_int(status_code) be equal to (0);
            
            t_file_metadata *metadata_after_commit = read_file_metadata(g_storage_config->mount_point, name, tag);
            should_string(metadata_after_commit->state) be equal to ((char*)COMMITTED);

            if (metadata_after_commit) destroy_file_metadata(metadata_after_commit);
            should_bool(correct_unlock(name, tag)) be truthy;
            package_destroy(response);
            package_destroy(request_package);
        } end

        it ("El manejador falla en la ejecucion (FILE_TAG_MISSING) y devuelve el código de error") {
            char *name = "file2";
            char *tag = "tag2";
            
            t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
            package_add_uint32(request_package, (uint32_t)301);
            package_add_string(request_package, name);
            package_add_string(request_package, tag);
            package_simulate_reception(request_package);

            t_package *response = handle_tag_commit_request(request_package);

            should_ptr(response) not be null;
            should_int(response->operation_code) be equal to (STORAGE_OP_TAG_COMMIT_RES);
            
            int8_t err_code;
            package_read_int8(response, &err_code);
            should_int(err_code) be equal to (FILE_TAG_MISSING);
            
            should_bool(correct_unlock(name, tag)) be truthy;
            
            package_destroy(response);
            package_destroy(request_package);
        } end

        it("Falla la deserializacion (paquete incompleto) y devuelve NULL") {
            // Paquete incompleto que falla al leer el tag
            t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
            package_add_uint32(request_package, (uint32_t)302);
            package_add_string(request_package, "file_ok");
            package_simulate_reception(request_package);

            t_package *response = handle_tag_commit_request(request_package);

            should_ptr(response) be null;

            package_destroy(request_package);
        } end
    } end
}