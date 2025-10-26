#include "../src/errors.h"
#include "../src/fresh_start/fresh_start.h"
#include "../src/globals/globals.h"
#include "../src/operations/create_file.h"
#include "../src/operations/delete_tag.h"
#include "../src/utils/filesystem_utils.h"
#include "test_utils.h"
#include <cspecs/cspec.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

context(test_delete_tag) {
  describe("delete_tag") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;

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
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("elimina tag exitosamente") {
      _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);

      int result = delete_tag(2, "test_file", "v1", TEST_MOUNT_POINT);
      should_int(result) be equal to(0);

      char tag_dir[PATH_MAX];
      snprintf(tag_dir, sizeof(tag_dir), "%s/files/test_file/v1",
               TEST_MOUNT_POINT);
      should_bool(directory_exists(tag_dir)) be falsey;
    }
    end

    it("retorna FILE_TAG_MISSING cuando el tag no existe") {
      int result = delete_tag(3, "nonexistent", "v1", TEST_MOUNT_POINT);

      should_int(result) be equal to(FILE_TAG_MISSING);
    }
    end

    it("retorna error al intentar eliminar initial_file:BASE") {
      int result = delete_tag(11, "initial_file", "BASE", TEST_MOUNT_POINT);

      should_int(result) be equal to(-1);

      char tag_dir[PATH_MAX];
      snprintf(tag_dir, sizeof(tag_dir), "%s/files/initial_file/BASE",
               TEST_MOUNT_POINT);
      should_bool(directory_exists(tag_dir)) be truthy;
    }
    end
  }
  end
}
