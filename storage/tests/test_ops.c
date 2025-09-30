#include <cspecs/cspec.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "../src/ops.h"
#include "../src/fresh_start/fresh_start.h"
#include "../src/globals/globals.h"
#include "test_utils.h"

context(test_ops) {
    describe("create_file op") {
        t_log* test_logger;

        before {
            create_test_directory();
            test_logger = create_test_logger();

            // Crear estructura básica del filesystem para las pruebas
            char files_dir[PATH_MAX];
            snprintf(files_dir, sizeof(files_dir), "%s/files", TEST_MOUNT_POINT);
            mkdir(files_dir, 0755);
        } end

        after {
            destroy_test_logger(test_logger);
            cleanup_test_directory();
        } end

        it("crea archivo nuevo con tag correctamente") {
            int result = create_file("test_file", "v1", TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            // Verificar estructura de carpetas creada
            char file_dir[PATH_MAX];
            snprintf(file_dir, sizeof(file_dir), "%s/files/test_file", TEST_MOUNT_POINT);
            should_bool(directory_exists(file_dir)) be truthy;

            char tag_dir[PATH_MAX];
            snprintf(tag_dir, sizeof(tag_dir), "%s/files/test_file/v1", TEST_MOUNT_POINT);
            should_bool(directory_exists(tag_dir)) be truthy;

            char logical_blocks_dir[PATH_MAX];
            snprintf(logical_blocks_dir, sizeof(logical_blocks_dir), "%s/files/test_file/v1/logical_blocks", TEST_MOUNT_POINT);
            should_bool(directory_exists(logical_blocks_dir)) be truthy;

            // Verificar archivo de metadata
            char metadata_file[PATH_MAX];
            snprintf(metadata_file, sizeof(metadata_file), "%s/files/test_file/v1/metadata.config", TEST_MOUNT_POINT);
            should_bool(file_exists(metadata_file)) be truthy;

            // Verificar contenido de metadata
            char metadata_content[256];
            read_file_contents(metadata_file, metadata_content, sizeof(metadata_content));
            should_ptr(strstr(metadata_content, "SIZE=0")) not be null;
            should_ptr(strstr(metadata_content, "BLOCKS=[]")) not be null;
            should_ptr(strstr(metadata_content, "ESTADO=WORK_IN_PROGRESS")) not be null;
        } end

        it("retorna error si ya existe la carpeta del archivo con el mismo tag") {
            create_file("existing_file", "existing_tag", TEST_MOUNT_POINT, test_logger);

            int result = create_file("existing_file", "existing_tag", TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(-1);
        } end

        it("crea multiples tags para el mismo archivo") {
            create_file("multi_tag_file", "tag1", TEST_MOUNT_POINT, test_logger);
            int result = create_file("multi_tag_file", "tag2", TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            char tag1_dir[PATH_MAX], tag2_dir[PATH_MAX];
            snprintf(tag1_dir, sizeof(tag1_dir), "%s/files/multi_tag_file/tag1", TEST_MOUNT_POINT);
            snprintf(tag2_dir, sizeof(tag2_dir), "%s/files/multi_tag_file/tag2", TEST_MOUNT_POINT);

            should_bool(directory_exists(tag1_dir)) be truthy;
            should_bool(directory_exists(tag2_dir)) be truthy;
        } end
    } end

    describe("truncate_file op") {
        t_log* test_logger;

        before {
            create_test_directory();
            test_logger = create_test_logger();
            g_storage_logger = test_logger;  // Para que init_storage use el logger correcto

            // Crear superblock.config y usar fresh start del storage
            create_test_superblock(TEST_MOUNT_POINT);
            init_storage(TEST_MOUNT_POINT);
        } end

        after {
            destroy_test_logger(test_logger);
            cleanup_test_directory();
        } end

        it("trunca archivo reduciendo el tamaño correctamente") {
            // Crear archivo de prueba
            create_file("test_truncate", "v1", TEST_MOUNT_POINT, test_logger);

            // Simular archivo con 3 bloques
            char metadata_path[PATH_MAX];
            snprintf(metadata_path, sizeof(metadata_path), "%s/files/test_truncate/v1/metadata.config", TEST_MOUNT_POINT);
            FILE* metadata = fopen(metadata_path, "w");
            fprintf(metadata, "SIZE=384\nBLOCKS=[1,2,3]\nESTADO=WORK_IN_PROGRESS\n");
            fclose(metadata);

            // Truncar a 2 bloques (256 bytes)
            int result = truncate_file("test_truncate", "v1", 256, TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            // Verificar que la metadata se actualizó correctamente
            char metadata_content[256];
            read_file_contents(metadata_path, metadata_content, sizeof(metadata_content));
            should_ptr(strstr(metadata_content, "SIZE=256")) not be null;
            should_ptr(strstr(metadata_content, "BLOCKS=[1,2]")) not be null;
        } end

        it("expande archivo creando nuevos bloques correctamente") {
            // Crear archivo de prueba con 1 bloque
            create_file("test_expand", "v1", TEST_MOUNT_POINT, test_logger);

            char metadata_path[PATH_MAX];
            snprintf(metadata_path, sizeof(metadata_path), "%s/files/test_expand/v1/metadata.config", TEST_MOUNT_POINT);
            FILE* metadata = fopen(metadata_path, "w");
            fprintf(metadata, "SIZE=128\nBLOCKS=[1]\nESTADO=WORK_IN_PROGRESS\n");
            fclose(metadata);

            // Expandir a 3 bloques (384 bytes)
            int result = truncate_file("test_expand", "v1", 384, TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            // Verificar que la metadata se actualizó correctamente
            char metadata_content[256];
            read_file_contents(metadata_path, metadata_content, sizeof(metadata_content));
            should_ptr(strstr(metadata_content, "SIZE=384")) not be null;
            should_ptr(strstr(metadata_content, "BLOCKS=[1,0,0]")) not be null;
        } end

        it("retorna error para archivo inexistente") {
            int result = truncate_file("nonexistent", "v1", 256, TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(-2);
        } end

        it("retorna error si no puede abrir superblock config") {
            int result = truncate_file("test", "v1", 256, "/invalid/path", test_logger);

            should_int(result) be equal to(-1);
        } end

        it("no modifica nada si el nuevo tamaño encaja en la misma cantidad de bloques") {
            // Crear archivo de prueba
            create_file("test_same_blocks", "v1", TEST_MOUNT_POINT, test_logger);

            char metadata_path[PATH_MAX];
            snprintf(metadata_path, sizeof(metadata_path), "%s/files/test_same_blocks/v1/metadata.config", TEST_MOUNT_POINT);
            FILE* metadata = fopen(metadata_path, "w");
            fprintf(metadata, "SIZE=200\nBLOCKS=[1,2]\nESTADO=WORK_IN_PROGRESS\n");
            fclose(metadata);

            // Cambiar a un tamaño que sigue necesitando 2 bloques
            int result = truncate_file("test_same_blocks", "v1", 250, TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            // El resultado debería ser que no se modificó nada
            char metadata_content[256];
            read_file_contents(metadata_path, metadata_content, sizeof(metadata_content));
            should_ptr(strstr(metadata_content, "BLOCKS=[1,2]")) not be null;
        } end
    } end
}
