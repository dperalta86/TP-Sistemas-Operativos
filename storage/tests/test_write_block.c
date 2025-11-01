#include <commons/log.h>
#include <connection/serialization.h>
#include <connection/protocol.h>
#include <operations/write_block.h>
#include <config/storage_config.h>
#include <globals/globals.h>
#include <fresh_start/fresh_start.h>
#include "test_utils.h"
#include <cspecs/cspec.h>

context(tests_write_block) {   
    before {
        g_storage_logger = create_test_logger();
    } end

    after {
        destroy_test_logger(g_storage_logger);
    } end

    describe("Deserialización de datos recibidos de cliente") {        
        it("Deserializa correctamente todos los datos") {
            uint32_t query_id;
            char *name = NULL;
            char *tag = NULL;
            uint32_t block_number;
            char *block_content = NULL;

            t_package *package = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);

            package_add_uint32(package, 12);
            package_add_string(package, "file");
            package_add_string(package, "tag1");
            package_add_uint32(package, 21);
            package_add_string(package, "TEXTO");

            int retval = deserialize_block_write_request(package, &query_id, &name, &tag, &block_number, &block_content);
            should_int(retval) be equal to (0);

            if (package) free(package);
            if (name) free(name);
            if (tag) free(tag);
            if (block_content) free(block_content);
        } end
    } end

    describe("Creación de hardlinks") {
        before {
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
        } end

        it("Crea hardlink exitosamente") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path), "%s/path.dat", TEST_MOUNT_POINT);
            int retval = create_new_hardlink(12, logical_block_path, 2);

            should_int(retval) be equal to (0);
        } end

        it("Index de bloque físico fuera de los límites") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path), "%s/path.dat", TEST_MOUNT_POINT);
            int retval = create_new_hardlink(12, logical_block_path, 80);

            should_int(retval) be equal to (-1);
        } end

        it("Error al crear el hardlink") {
            char logical_block_path[PATH_MAX];
            snprintf(logical_block_path, sizeof(logical_block_path), TEST_MOUNT_POINT);
            int retval = create_new_hardlink(12, logical_block_path, 2);

            should_int(retval) be equal to (-1);
        } end
    } end
} 
