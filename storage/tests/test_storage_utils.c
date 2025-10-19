#include "../src/globals/globals.h"
#include "../src/utils/filesystem_utils.h"
#include "test_utils.h"
#include <cspecs/cspec.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

context(test_storage_utils) {
  describe("read_superblock function") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;
    }
    end

        after {
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("lee superblock.config con contenido correcto") {
      create_test_superblock(TEST_MOUNT_POINT);

      int fs_size, block_size;
      int result = read_superblock(TEST_MOUNT_POINT, &fs_size, &block_size);

      should_int(result) be equal to(0);
      should_int(fs_size) be equal to(TEST_FS_SIZE);
      should_int(block_size) be equal to(TEST_BLOCK_SIZE);
    }
    end

    it("retorna error para archivo inexistente") {
      int fs_size, block_size;
      int result = read_superblock(TEST_MOUNT_POINT, &fs_size, &block_size);

      should_int(result) be equal to(-1);
    }
    end

    it("retorna error para archivo malformado") {
      char superblock_path[PATH_MAX];
      snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config",
               TEST_MOUNT_POINT);

      FILE *superblock_file = fopen(superblock_path, "w");
      fprintf(superblock_file, "PROPIEDAD_CUALQUIERA=123\n");
      fclose(superblock_file);

      int fs_size, block_size;
      int result = read_superblock(TEST_MOUNT_POINT, &fs_size, &block_size);

      should_int(result) be equal to(-2);
    }
    end

    it("retorna error para archivo con propiedades faltantes") {
      char superblock_path[PATH_MAX];
      snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config",
               TEST_MOUNT_POINT);

      FILE *superblock_file = fopen(superblock_path, "w");
      fprintf(superblock_file, "FS_SIZE=%d\n",
              TEST_FS_SIZE); // Falta BLOCK_SIZE
      fclose(superblock_file);

      int fs_size, block_size;
      int result = read_superblock(TEST_MOUNT_POINT, &fs_size, &block_size);

      should_int(result) be equal to(-2);
    }
    end
  }
  end

    describe("get_bitmap_size_bytes function") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;
    }
    end

        after {
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("calcula correctamente el tamaño del bitmap") {
      create_test_superblock(TEST_MOUNT_POINT);

      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);

      // Con TEST_FS_SIZE=4096 y TEST_BLOCK_SIZE=128, necesitamos 32 bloques
      // 32 bits = 4 bytes
      size_t expected_size = (TEST_FS_SIZE / TEST_BLOCK_SIZE + 7) / 8;
      should_int((int)bitmap_size) be equal to((int)expected_size);
    }
    end

    it("retorna error para superblock inexistente") {
      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);

      should_int((int)bitmap_size) be equal to(-1);
    }
    end

    it("retorna error para path inexistente") {
      size_t bitmap_size = get_bitmap_size_bytes("/path/inexistente");

      should_int((int)bitmap_size) be equal to(-1);
    }
    end
  }
  end

    describe("modify_bitmap_bits function") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;
    }
    end

        after {
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("setea bits correctamente") {
      create_test_superblock(TEST_MOUNT_POINT);

      // Crear bitmap inicial con todos los bits en 0
      char bitmap_path[PATH_MAX];
      snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin",
               TEST_MOUNT_POINT);

      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);
      unsigned char *bitmap_data = calloc(bitmap_size, 1);

      FILE *bitmap_file = fopen(bitmap_path, "wb");
      fwrite(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      int result = modify_bitmap_bits(TEST_MOUNT_POINT, 0, 3, 1);

      should_int(result) be equal to(0);

      // Verificar que los bits se setearon correctamente
      bitmap_file = fopen(bitmap_path, "rb");
      fread(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      // Los bits 0, 1, 2 deberían estar en 1
      should_int(bitmap_data[0] & 0x80) not be equal to(0);
      should_int(bitmap_data[0] & 0x40) not be equal to(0);
      should_int(bitmap_data[0] & 0x20) not be equal to(0);

      free(bitmap_data);
    }
    end

    it("unsetea bits correctamente") {
      create_test_superblock(TEST_MOUNT_POINT);

      char bitmap_path[PATH_MAX];
      snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin",
               TEST_MOUNT_POINT);

      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);
      unsigned char *bitmap_data = malloc(bitmap_size);
      memset(bitmap_data, 0xFF, bitmap_size);

      FILE *bitmap_file = fopen(bitmap_path, "wb");
      fwrite(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      // Unsetear los bits 3-5
      int result = modify_bitmap_bits(TEST_MOUNT_POINT, 3, 3, 0);

      should_int(result) be equal to(0);

      bitmap_file = fopen(bitmap_path, "rb");
      fread(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      // Los bits 3, 4, 5 deberían estar en 0
      should_int(bitmap_data[0] & 0x10) be equal to(0);
      should_int(bitmap_data[0] & 0x08) be equal to(0);
      should_int(bitmap_data[0] & 0x04) be equal to(0);

      free(bitmap_data);
    }
    end

    it("retorna error para superblock inexistente") {
      int result = modify_bitmap_bits(TEST_MOUNT_POINT, 0, 2, 1);

      should_int(result) be equal to(-1);
    }
    end

    it("retorna error para bitmap inexistente") {
      create_test_superblock(TEST_MOUNT_POINT);

      int result = modify_bitmap_bits(TEST_MOUNT_POINT, 0, 2, 1);

      should_int(result) be equal to(-1);
    }
    end

    it("maneja array vacío correctamente") {
      create_test_superblock(TEST_MOUNT_POINT);

      char bitmap_path[PATH_MAX];
      snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin",
               TEST_MOUNT_POINT);

      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);
      unsigned char *bitmap_data = calloc(bitmap_size, 1);

      FILE *bitmap_file = fopen(bitmap_path, "wb");
      fwrite(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      // Llamar con count = 0
      int result = modify_bitmap_bits(TEST_MOUNT_POINT, 0, 0, 1);

      should_int(result) be equal to(0);

      free(bitmap_data);
    }
    end
  }
  end
}
