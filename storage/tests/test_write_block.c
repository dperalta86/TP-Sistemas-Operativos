#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include <connection/serialization.h>
#include <connection/protocol.h>
#include <operations/write_block.h>
#include <config/storage_config.h>
#include <globals/globals.h>
#include <fresh_start/fresh_start.h>
#include "test_utils.h"
#include <cspecs/cspec.h>
#include <string.h>

context(tests_write_block) {   

    describe("Deserialización de datos recibidos de cliente") {        
        it("Deserializa correctamente todos los datos") {
            uint32_t query_id;
            char *name = NULL;
            char *tag = NULL;
            uint32_t block_number;
            void *block_data = NULL;
            size_t data_size = 0;

            t_package *package = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);

            const char *content = "TEXTO";
            size_t content_size = strlen(content);

            package_add_uint32(package, 12);
            package_add_string(package, "file");
            package_add_string(package, "tag1");
            package_add_uint32(package, 21);
            package_add_data(package, content, content_size);

            package_reset_read_offset(package);

            int retval = deserialize_block_write_request(
                package,
                &query_id,
                &name,
                &tag,
                &block_number,
                &block_data,
                &data_size
            );
            should_int(retval) be equal to (0);
            should_int(query_id) be equal to (12);
            should_int(block_number) be equal to (21);
            should_int(data_size) be equal to ((int)content_size);
            should_bool(memcmp(block_data, content, content_size) == 0) be truthy;

            if (package) package_destroy(package);
            if (name) free(name);
            if (tag) free(tag);
            if (block_data) free(block_data);
        } end
    } end

    describe("Creación de hardlinks") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 1000, 1000, "INFO");
            create_test_superblock(TEST_MOUNT_POINT);

            char config_path[PATH_MAX];
            snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
            g_storage_config = create_storage_config(config_path);

            init_physical_blocks(TEST_MOUNT_POINT, g_storage_config->fs_size, g_storage_config->block_size);
        } end

        after {
            destroy_storage_config(g_storage_config);
            cleanup_test_directory();
            destroy_test_logger(g_storage_logger);
        } end

        it("Crea hardlink exitosamente") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path), "%s/%04d.dat", TEST_MOUNT_POINT, 2);
            int retval = create_new_hardlink(12, "file1", "tag1", 2, logical_block_path, 2);

            should_int(retval) be equal to (0);
        } end

        it("Index de bloque físico fuera de los límites") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path), "%s/%04d.dat", TEST_MOUNT_POINT, 2);
            int retval = create_new_hardlink(12, "file1", "tag1", 2, logical_block_path, 80);

            should_int(retval) be equal to (-1);
        } end

        it("Error al crear el hardlink") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path), TEST_MOUNT_POINT);
            int retval = create_new_hardlink(12, "file1", "tag1", 2, logical_block_path, 2);

            should_int(retval) be equal to (-1);
        } end
    } end

    describe("Escritura en bloque físico") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 1000, 1000, "INFO");
            create_test_superblock(TEST_MOUNT_POINT);

            char config_path[PATH_MAX];
            snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
            g_storage_config = create_storage_config(config_path);

            init_physical_blocks(TEST_MOUNT_POINT, g_storage_config->fs_size, g_storage_config->block_size);
            init_logical_blocks("file1", "tag1", 20, TEST_MOUNT_POINT);
        } end

        after {
            destroy_storage_config(g_storage_config);
            cleanup_test_directory();
            destroy_test_logger(g_storage_logger);
        } end

        it ("Escribe en bloque físico exitosamente") {
            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = write_to_logical_block(12, "file1", "tag1", 3, content, content_size);
            should_int(retval) be equal to (0);
        } end

        it ("No halla el archivo de bloque lógico") {
            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = write_to_logical_block(12, "file1", "tag1", 93, content, content_size);
            should_int(retval) be equal to (-1);
        } end
    } end

    describe ("Lógica central de escritura en bloques") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 1000, 1000, "INFO");
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

        it ("El file:tag no existe") {
            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 3, content, content_size);

            should_int(retval) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end

        it ("Fallo en la lectura de metadata devuelve código FILE_TAG_MISSING") {
            init_logical_blocks("file1", "tag1", 20, TEST_MOUNT_POINT);

            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 3, content, content_size);

            should_int(retval) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end

        it ("file:tag ya estaba en estado COMMITTED") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "COMMITTED", TEST_MOUNT_POINT);

            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 1, content, content_size);

            should_int(retval) be equal to (FILE_ALREADY_COMMITTED);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end

        it ("Bloque lógico fuera de rango") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "WORK_IN_PROGRESS", TEST_MOUNT_POINT);

            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 7, content, content_size);

            should_int(retval) be equal to (READ_OUT_OF_BOUNDS);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end

        it ("Falla la apertura del bitmap") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "WORK_IN_PROGRESS", TEST_MOUNT_POINT);

            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 1, content, content_size);

            should_int(retval) be equal to (-3);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end

        it ("No hay más bloques físicos libres") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "WORK_IN_PROGRESS", TEST_MOUNT_POINT);
            init_bitmap(TEST_MOUNT_POINT, TEST_FS_SIZE, TEST_BLOCK_SIZE);
            size_t numb_blocks = g_storage_config->bitmap_size_bytes * (size_t)8;
            modify_bitmap_bits(TEST_MOUNT_POINT, 0, numb_blocks, 1);

            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 1, content, content_size);

            should_int(retval) be equal to (NOT_ENOUGH_SPACE);
            int lock_res = mutex_is_free(&g_storage_bitmap_mutex);
            should_int(lock_res) be equal to (0);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end

        it ("Escritura en bloque exitosa") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "WORK_IN_PROGRESS", TEST_MOUNT_POINT);
            init_bitmap(TEST_MOUNT_POINT, TEST_FS_SIZE, TEST_BLOCK_SIZE);

            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            int retval = execute_block_write("file1", "tag1", 12, 1, content, content_size);

            should_int(retval) be equal to (0);
            int lock_res = mutex_is_free(&g_storage_bitmap_mutex);
            should_int(lock_res) be equal to (0);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
        } end
    } end

    describe ("Lógica que maneja la solicitud de escritura en bloque") {
        before {
            g_storage_logger = create_test_logger();
            create_test_directory();
            create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 1000, 1000, "INFO");
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

        it ("El manejador falla y devuelve un paquete de respuesta con código de error") {
            t_package *package = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);
            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            package_add_uint32(package, (uint32_t)12);
            package_add_string(package, "file1");
            package_add_string(package, "tag1");
            package_add_uint32(package, (uint32_t)3);
            package_add_data(package, content, content_size);
            package_reset_read_offset(package);

            t_package *response = handle_write_block_request(package);

            should_int(response->operation_code) be equal to (STORAGE_OP_BLOCK_WRITE_RES);
            int8_t err_code;
            package_read_int8(response, &err_code);
            should_int(err_code) be equal to (FILE_TAG_MISSING);
            should_int(mutex_is_free(&g_storage_bitmap_mutex)) be equal to (0);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
            package_destroy(response);
            package_destroy(package);
        } end

        it ("Se maneja la escritura de bloque exitosamente y se devuelve un paquete de respuesta con código 0") {
            init_logical_blocks("file1", "tag1", 3, TEST_MOUNT_POINT);
            create_test_metadata("file1", "tag1", 3, "[1,2,3]", "WORK_IN_PROGRESS", TEST_MOUNT_POINT);
            init_bitmap(TEST_MOUNT_POINT, TEST_FS_SIZE, TEST_BLOCK_SIZE);

            t_package *package = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);
            const char *content = "CONTENIDO";
            size_t content_size = strlen(content);

            package_add_uint32(package, (uint32_t)12);
            package_add_string(package, "file1");
            package_add_string(package, "tag1");
            package_add_uint32(package, (uint32_t)2);
            package_add_data(package, content, content_size);
            package_reset_read_offset(package);

            t_package *response = handle_write_block_request(package);

            should_int(response->operation_code) be equal to (STORAGE_OP_BLOCK_WRITE_RES);
            int8_t success_code;
            package_read_int8(response, &success_code);
            should_int(success_code) be equal to (0);
            should_int(mutex_is_free(&g_storage_bitmap_mutex)) be equal to (0);
            should_bool(correct_unlock("file1", "tag1")) be truthy;
            package_destroy(response);
            package_destroy(package);
        } end

    } end
} 
