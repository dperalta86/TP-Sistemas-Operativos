#include "../src/errors.h"
#include "../src/fresh_start/fresh_start.h"
#include "../src/globals/globals.h"
#include "../src/operations/create_file.h"
#include "../src/utils/filesystem_utils.h"
#include "test_utils.h"
#include <cspecs/cspec.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Declaraciones para funciones static que testeamos
int truncate_file(uint32_t query_id, const char *name, const char *tag,
                  int new_size_bytes, const char *mount_point);
int maybe_handle_orphaned_physical_block(const char *physical_block_path,
                                         const char *mount_point,
                                         uint32_t query_id);

context(test_ops) {
  describe("create_file op") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;

      // Crear estructura básica del filesystem para las pruebas
      char files_dir[PATH_MAX];
      snprintf(files_dir, sizeof(files_dir), "%s/files", TEST_MOUNT_POINT);
      mkdir(files_dir, 0755);
    }
    end

        after {
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("crea archivo nuevo con tag correctamente") {
      int result = _create_file(1, "test_file", "v1", TEST_MOUNT_POINT);

      should_int(result) be equal to(0);

      // Verificar estructura de carpetas creada
      char file_dir[PATH_MAX];
      snprintf(file_dir, sizeof(file_dir), "%s/files/test_file",
               TEST_MOUNT_POINT);
      should_bool(directory_exists(file_dir)) be truthy;

      char tag_dir[PATH_MAX];
      snprintf(tag_dir, sizeof(tag_dir), "%s/files/test_file/v1",
               TEST_MOUNT_POINT);
      should_bool(directory_exists(tag_dir)) be truthy;

      char logical_blocks_dir[PATH_MAX];
      snprintf(logical_blocks_dir, sizeof(logical_blocks_dir),
               "%s/files/test_file/v1/logical_blocks", TEST_MOUNT_POINT);
      should_bool(directory_exists(logical_blocks_dir)) be truthy;

      // Verificar archivo de metadata
      char metadata_file[PATH_MAX];
      snprintf(metadata_file, sizeof(metadata_file),
               "%s/files/test_file/v1/metadata.config", TEST_MOUNT_POINT);
      should_bool(file_exists(metadata_file)) be truthy;

      // Verificar contenido de metadata
      char metadata_content[256];
      read_file_contents(metadata_file, metadata_content,
                         sizeof(metadata_content));
      should_ptr(strstr(metadata_content, "SIZE=0")) not be null;
      should_ptr(strstr(metadata_content, "BLOCKS=[]")) not be null;
      should_ptr(strstr(metadata_content, "ESTADO=WORK_IN_PROGRESS"))
          not be null;
    }
    end

    it("retorna error si ya existe la carpeta del archivo con el mismo tag") {
      _create_file(2, "existing_file", "existing_tag", TEST_MOUNT_POINT);

      int result =
          _create_file(3, "existing_file", "existing_tag", TEST_MOUNT_POINT);

      should_int(result) be equal to(FILE_TAG_ALREADY_EXISTS);
    }
    end

    it("crea multiples tags para el mismo archivo") {
      _create_file(4, "multi_tag_file", "tag1", TEST_MOUNT_POINT);
      int result = _create_file(5, "multi_tag_file", "tag2", TEST_MOUNT_POINT);

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

    describe("truncate_file op") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger =
          test_logger; // Para que init_storage use el logger correcto

      // Crear superblock.config y usar fresh start del storage
      create_test_superblock(TEST_MOUNT_POINT);
      init_storage(TEST_MOUNT_POINT);
    }
    end

        after {
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("trunca archivo reduciendo el tamaño correctamente") {
      _create_file(6, "test_truncate", "v1", TEST_MOUNT_POINT);

      // Simular archivo con 3 bloques
      char metadata_path[PATH_MAX];
      snprintf(metadata_path, sizeof(metadata_path),
               "%s/files/test_truncate/v1/metadata.config", TEST_MOUNT_POINT);
      FILE *metadata = fopen(metadata_path, "w");
      fprintf(metadata, "SIZE=384\nBLOCKS=[1,2,3]\nESTADO=WORK_IN_PROGRESS\n");
      fclose(metadata);

      // Truncar a 2 bloques (256 bytes)
      int result =
          truncate_file(7, "test_truncate", "v1", 256, TEST_MOUNT_POINT);

      should_int(result) be equal to(0);

      // Verificar que la metadata se actualizó correctamente
      char metadata_content[256];
      read_file_contents(metadata_path, metadata_content,
                         sizeof(metadata_content));
      should_ptr(strstr(metadata_content, "SIZE=256")) not be null;
      should_ptr(strstr(metadata_content, "BLOCKS=[1,2]")) not be null;
    }
    end

    it("expande archivo creando nuevos bloques correctamente") {
      // Crear archivo de prueba con 1 bloque
      _create_file(8, "test_expand", "v1", TEST_MOUNT_POINT);

      char metadata_path[PATH_MAX];
      snprintf(metadata_path, sizeof(metadata_path),
               "%s/files/test_expand/v1/metadata.config", TEST_MOUNT_POINT);
      FILE *metadata = fopen(metadata_path, "w");
      fprintf(metadata, "SIZE=128\nBLOCKS=[1]\nESTADO=WORK_IN_PROGRESS\n");
      fclose(metadata);

      // Expandir a 3 bloques (384 bytes)
      int result = truncate_file(9, "test_expand", "v1", 384, TEST_MOUNT_POINT);

      should_int(result) be equal to(0);

      // Verificar que la metadata se actualizó correctamente
      char metadata_content[256];
      read_file_contents(metadata_path, metadata_content,
                         sizeof(metadata_content));
      should_ptr(strstr(metadata_content, "SIZE=384")) not be null;
      should_ptr(strstr(metadata_content, "BLOCKS=[1,0,0]")) not be null;
    }
    end

    it("retorna error para archivo inexistente") {
      int result =
          truncate_file(10, "nonexistent", "v1", 256, TEST_MOUNT_POINT);

      should_int(result) be equal to(-2);
    }
    end

    it("retorna error si no puede abrir superblock config") {
      int result = truncate_file(11, "test", "v1", 256, "/invalid/path");

      should_int(result) be equal to(-1);
    }
    end

    it("no modifica nada si el nuevo tamaño encaja en la misma cantidad de "
       "bloques") {
      // Crear archivo de prueba
      _create_file(12, "test_same_blocks", "v1", TEST_MOUNT_POINT);

      char metadata_path[PATH_MAX];
      snprintf(metadata_path, sizeof(metadata_path),
               "%s/files/test_same_blocks/v1/metadata.config",
               TEST_MOUNT_POINT);
      FILE *metadata = fopen(metadata_path, "w");
      fprintf(metadata, "SIZE=200\nBLOCKS=[1,2]\nESTADO=WORK_IN_PROGRESS\n");
      fclose(metadata);

      // Cambiar a un tamaño que sigue necesitando 2 bloques
      int result =
          truncate_file(13, "test_same_blocks", "v1", 250, TEST_MOUNT_POINT);

      should_int(result) be equal to(0);

      char metadata_content[256];
      read_file_contents(metadata_path, metadata_content,
                         sizeof(metadata_content));
      should_ptr(strstr(metadata_content, "BLOCKS=[1,2]")) not be null;
    }
    end
  }
  end

    describe("maybe_handle_orphaned_physical_block") {
    t_log *test_logger;

    before {
      create_test_directory();
      test_logger = create_test_logger();
      g_storage_logger = test_logger;

      create_test_superblock(TEST_MOUNT_POINT);
      init_storage(TEST_MOUNT_POINT);
    }
    end

        after {
      destroy_test_logger(test_logger);
      cleanup_test_directory();
    }
    end

    it("libera bloque huérfano sin hard links correctamente") {
      // Crear un bloque físico de prueba
      char physical_block_path[PATH_MAX];
      snprintf(physical_block_path, sizeof(physical_block_path),
               "%s/physical_blocks/block0001.dat", TEST_MOUNT_POINT);

      FILE *block_file = fopen(physical_block_path, "w");
      fprintf(block_file, "test data");
      fclose(block_file);

      // Setear el bit en el bitmap
      int block_index = 1;
      int result_set = modify_bitmap_bits(TEST_MOUNT_POINT, block_index, 1, 1);
      should_int(result_set) be equal to(0);

      int result = maybe_handle_orphaned_physical_block(physical_block_path,
                                                        TEST_MOUNT_POINT, 123);

      should_int(result) be equal to(0);

      // Verificar que el bit se unseteó en el bitmap
      char bitmap_path[PATH_MAX];
      snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin",
               TEST_MOUNT_POINT);

      FILE *bitmap_file = fopen(bitmap_path, "rb");
      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);
      unsigned char *bitmap_data = malloc(bitmap_size);
      fread(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      // El bit 1 debería estar en 0
      should_int(bitmap_data[0] & 0x40) be equal to(0); // bit 1

      free(bitmap_data);
    }
    end

    it("no libera bloque con hard links") {
      char physical_block_path[PATH_MAX];
      char hard_link_path[PATH_MAX];
      snprintf(physical_block_path, sizeof(physical_block_path),
               "%s/physical_blocks/block0002.dat", TEST_MOUNT_POINT);
      snprintf(hard_link_path, sizeof(hard_link_path), "%s/test_hardlink.dat",
               TEST_MOUNT_POINT);

      FILE *block_file = fopen(physical_block_path, "w");
      fprintf(block_file, "test data");
      fclose(block_file);

      // Crear hard link
      link(physical_block_path, hard_link_path);

      // Setear el bit en el bitmap
      int block_index = 2;
      int result_set = modify_bitmap_bits(TEST_MOUNT_POINT, block_index, 1, 1);
      should_int(result_set) be equal to(0);

      int result = maybe_handle_orphaned_physical_block(physical_block_path,
                                                        TEST_MOUNT_POINT, 456);

      should_int(result) be equal to(0);

      // Verificar que el bit NO se unseteó en el bitmap
      char bitmap_path[PATH_MAX];
      snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin",
               TEST_MOUNT_POINT);

      FILE *bitmap_file = fopen(bitmap_path, "rb");
      size_t bitmap_size = get_bitmap_size_bytes(TEST_MOUNT_POINT);
      unsigned char *bitmap_data = malloc(bitmap_size);
      fread(bitmap_data, 1, bitmap_size, bitmap_file);
      fclose(bitmap_file);

      // El bit 2 debería seguir en 1
      should_int(bitmap_data[0] & 0x20) not be equal to(0); // bit 2

      free(bitmap_data);
      unlink(hard_link_path);
    }
    end

    it("retorna error para archivo inexistente") {
      char nonexistent_path[PATH_MAX];
      snprintf(nonexistent_path, sizeof(nonexistent_path),
               "%s/physical_blocks/block9999.dat", TEST_MOUNT_POINT);

      int result = maybe_handle_orphaned_physical_block(nonexistent_path,
                                                        TEST_MOUNT_POINT, 789);

      should_int(result) be equal to(-1);
    }
    end

    it("retorna error para nombre de bloque inválido") {
      char invalid_path[PATH_MAX];
      snprintf(invalid_path, sizeof(invalid_path),
               "%s/physical_blocks/invalid_name.dat", TEST_MOUNT_POINT);

      FILE *block_file = fopen(invalid_path, "w");
      fprintf(block_file, "test data");
      fclose(block_file);

      int result = maybe_handle_orphaned_physical_block(invalid_path,
                                                        TEST_MOUNT_POINT, 101);

      should_int(result) be equal to(-2);
    }
    end

    it("retorna error para superblock inexistente") {
      char physical_block_path[PATH_MAX];
      snprintf(physical_block_path, sizeof(physical_block_path),
               "%s/physical_blocks/block0003.dat", TEST_MOUNT_POINT);

      FILE *block_file = fopen(physical_block_path, "w");
      fprintf(block_file, "test data");
      fclose(block_file);

      int result = maybe_handle_orphaned_physical_block(physical_block_path,
                                                        "/invalid/mount", 202);

      should_int(result) be equal to(-3);
    }
    end
  }
  end
}
