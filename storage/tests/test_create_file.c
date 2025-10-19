#include "../src/fresh_start/fresh_start.h"
#include "../src/globals/globals.h"
#include "../src/operations/create_file.h"
#include "../src/errors.h"
#include "test_utils.h"
#include <commons/config.h>
#include <cspecs/cspec.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

context(test_create_file) {
  describe("Operacion CREATE_FILE") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;
    }
    end

        after {
      g_storage_logger = NULL;
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

        describe("funcion _create_file"){
            before{create_test_superblock(TEST_MOUNT_POINT);
    init_storage(TEST_MOUNT_POINT);
  }
  end

            it("crea archivo nuevo con tag correctamente") {
    int result = _create_file(0, "test_file", "v1", TEST_MOUNT_POINT);

    should_int(result) be equal to(0);

    char target_path[PATH_MAX];

    // Verificar estructura de carpetas creada
    snprintf(target_path, sizeof(target_path), "%s/files/test_file",
             TEST_MOUNT_POINT);
    should_bool(directory_exists(target_path)) be truthy;

    snprintf(target_path, sizeof(target_path), "%s/files/test_file/v1",
             TEST_MOUNT_POINT);
    should_bool(directory_exists(target_path)) be truthy;

    snprintf(target_path, sizeof(target_path),
             "%s/files/test_file/v1/logical_blocks", TEST_MOUNT_POINT);
    should_bool(directory_exists(target_path)) be truthy;

    // Verificar archivo de metadata
    snprintf(target_path, sizeof(target_path),
             "%s/files/test_file/v1/metadata.config", TEST_MOUNT_POINT);
    should_bool(file_exists(target_path)) be truthy;

    // Verificar contenido de metadata
    char metadata_content[256];
    read_file_contents(target_path, metadata_content, sizeof(metadata_content));
    should_ptr(strstr(metadata_content, "SIZE=0")) not be null;
    should_ptr(strstr(metadata_content, "BLOCKS=[]")) not be null;
    should_ptr(strstr(metadata_content, "ESTADO=WORK_IN_PROGRESS")) not be null;
  }
  end

            it("retorna FILE_TAG_ALREADY_EXISTS si ya existe la carpeta del archivo con el mismo tag") {
    _create_file(0, "existing_file", "existing_tag", TEST_MOUNT_POINT);

    int result =
        _create_file(0, "existing_file", "existing_tag", TEST_MOUNT_POINT);

    should_int(result) be equal to(FILE_TAG_ALREADY_EXISTS);
  }
  end

            it("crea multiples tags para el mismo archivo") {
    _create_file(0, "multi_tag_file", "tag1", TEST_MOUNT_POINT);
    int result = _create_file(0, "multi_tag_file", "tag2", TEST_MOUNT_POINT);

    should_int(result) be equal to(0);

    char tag1_dir[PATH_MAX], tag2_dir[PATH_MAX];
    snprintf(tag1_dir, sizeof(tag1_dir), "%s/files/multi_tag_file/tag1",
             TEST_MOUNT_POINT);
    snprintf(tag2_dir, sizeof(tag2_dir), "%s/files/multi_tag_file/tag2",
             TEST_MOUNT_POINT);

    should_bool(directory_exists(tag1_dir)) be truthy;
    should_bool(directory_exists(tag2_dir)) be truthy;
  }
  end
}
end
}
end
}
