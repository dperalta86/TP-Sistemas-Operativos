#include "../src/errors.h"
#include "../src/fresh_start/fresh_start.h"
#include "../src/globals/globals.h"
#include "../src/operations/create_file.h"
#include "../src/operations/create_tag.h"
#include "../src/utils/filesystem_utils.h"
#include "test_utils.h"
#include <cspecs/cspec.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

context(test_create_tag) {
  describe("create_tag") {

    before {
      create_test_directory();
      g_storage_logger = create_test_logger();

      g_storage_config = malloc(sizeof(t_storage_config));
      g_storage_config->mount_point = strdup(TEST_MOUNT_POINT);
      g_storage_config->block_size = TEST_BLOCK_SIZE;
      g_storage_config->fs_size = TEST_FS_SIZE;
      int total_blocks =
          g_storage_config->fs_size / g_storage_config->block_size;
      g_storage_config->bitmap_size_bytes = (total_blocks + 7) / 8;

      g_open_files_dict = dictionary_create();

      create_test_superblock(TEST_MOUNT_POINT);
      init_storage(TEST_MOUNT_POINT);
    }
    end

        after {
      if (g_open_files_dict) {
        dictionary_destroy(g_open_files_dict);
        g_open_files_dict = NULL;
      }
      free(g_storage_config->mount_point);
      free(g_storage_config);
      g_storage_config = NULL;
      destroy_test_logger(g_storage_logger);
      cleanup_test_directory();
    }
    end

    it("crea tag exitosamente copiando desde tag existente") {
      _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);

      int result = create_tag(2, "test_file", "v1", "v2");
      should_int(result) be equal to(0);

      char tag_dir[PATH_MAX];
      snprintf(tag_dir, sizeof(tag_dir), "%s/files/test_file/v2",
               TEST_MOUNT_POINT);
      should_bool(directory_exists(tag_dir)) be truthy;

      char metadata_path[PATH_MAX];
      snprintf(metadata_path, sizeof(metadata_path),
               "%s/files/test_file/v2/metadata.config", TEST_MOUNT_POINT);
      should_bool(file_exists(metadata_path)) be truthy;
    }
    end

    it("convierte tag COMMITTED a WORK_IN_PROGRESS al copiar") {
      _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);

      t_file_metadata *src_metadata =
          read_file_metadata(TEST_MOUNT_POINT, "test_file", "v1");
      if (src_metadata->state) {
        free(src_metadata->state);
      }
      src_metadata->state = strdup("COMMITTED");
      save_file_metadata(src_metadata);
      destroy_file_metadata(src_metadata);

      int result = create_tag(2, "test_file", "v1", "v2");
      should_int(result) be equal to(0);

      t_file_metadata *dst_metadata =
          read_file_metadata(TEST_MOUNT_POINT, "test_file", "v2");
      should_ptr(dst_metadata) not be null;
      should_string(dst_metadata->state) be equal to("WORK_IN_PROGRESS");

      destroy_file_metadata(dst_metadata);
    }
    end

    it("retorna error cuando el tag destino ya existe") {
      _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);
      _create_file(2, "test_file", "v2", TEST_MOUNT_POINT);

      int result = create_tag(3, "test_file", "v1", "v2");
      should_int(result) be equal to(FILE_TAG_ALREADY_EXISTS);
    }
    end

    it("retorna error cuando el tag origen no existe") {
      _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);

      int result = create_tag(4, "test_file", "nonexistent", "v2");
      should_int(result) be equal to(-2);
    }
    end

    it("copia estructura completa incluyendo logical_blocks") {
      _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);

      int result = create_tag(2, "test_file", "v1", "v2");
      should_int(result) be equal to(0);

      char dst_blocks_dir[PATH_MAX];
      snprintf(dst_blocks_dir, sizeof(dst_blocks_dir),
               "%s/files/test_file/v2/logical_blocks", TEST_MOUNT_POINT);
      should_bool(directory_exists(dst_blocks_dir)) be truthy;
    }
    end
  }
  end
}
