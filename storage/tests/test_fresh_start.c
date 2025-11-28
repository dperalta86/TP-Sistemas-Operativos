#include "../src/fresh_start/fresh_start.h"
#include "../src/utils/filesystem_utils.h"
#include "globals/globals.h"
#include "test_utils.h"
#include <commons/bitarray.h>
#include <commons/config.h>
#include <cspecs/cspec.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

context(test_storage_filesystem) {
  describe("Funciones de Inicializacion del Filesystem"){

      before{create_test_directory();
  g_storage_logger = create_test_logger();
}
end

    after {
  destroy_test_logger(g_storage_logger);
  cleanup_test_directory();
}
end

    describe("funcion wipe_storage_content"){
        it("limpia exitosamente un directorio vacio"){
            int result = wipe_storage_content(TEST_MOUNT_POINT);

should_int(result) be equal to(0);
should_bool(is_directory_empty(TEST_MOUNT_POINT)) be truthy;
}
end

            it("limpia un directorio con archivos pero preserva superblock.config") {
  // Crear superblock.config
  create_test_superblock(TEST_MOUNT_POINT);

  // Crear algunos archivos de prueba
  char test_file_path[PATH_MAX];
  snprintf(test_file_path, sizeof(test_file_path), "%s/test_file.txt",
           TEST_MOUNT_POINT);
  FILE *test_file = fopen(test_file_path, "w");
  fprintf(test_file, "test");
  fclose(test_file);

  char superblock_path[PATH_MAX];
  snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config",
           TEST_MOUNT_POINT);

  int result = wipe_storage_content(TEST_MOUNT_POINT);

  should_int(result) be equal to(0);
  should_bool(file_exists(test_file_path)) be falsey;
  should_bool(file_exists(superblock_path)) be truthy;
}
end

            it("retorna error para directorio inexistente") {
  int result = wipe_storage_content("/non/existent/path");

  should_int(result) be equal to(-1);
}
end
}
end

    describe("funcion read_superblock"){
        it("lee superblock.config con contenido correcto"){
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
  fprintf(superblock_file, "INVALID_CONTENT=123\n");
  fclose(superblock_file);

  int fs_size, block_size;
  int result = read_superblock(TEST_MOUNT_POINT, &fs_size, &block_size);

  should_int(result) be equal to(-2);
}
end
}
end

    describe("funcion init_bitmap"){
        it("crea bitmap.bin con inicializacion correcta"){
            create_test_superblock(TEST_MOUNT_POINT);

int result = init_bitmap(TEST_MOUNT_POINT, TEST_FS_SIZE, TEST_BLOCK_SIZE);

should_int(result) be equal to(0);

char bitmap_path[PATH_MAX];
snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin", TEST_MOUNT_POINT);

should_bool(file_exists(bitmap_path)) be truthy;

// Verificar contenido del bitmap
int total_blocks = TEST_FS_SIZE / TEST_BLOCK_SIZE;
size_t bitmap_size_bytes = (total_blocks + 7) / 8; // Redondear al próximo byte

// Leer archivo bitmap en memoria
FILE *bitmap_file = fopen(bitmap_path, "rb");
should_ptr(bitmap_file) not be null;

char *bitmap_data = malloc(bitmap_size_bytes);
should_ptr(bitmap_data) not be null;

size_t bytes_read = fread(bitmap_data, 1, bitmap_size_bytes, bitmap_file);
should_int(bytes_read) be equal to(bitmap_size_bytes);
fclose(bitmap_file);

t_bitarray *bitmap =
    bitarray_create_with_mode(bitmap_data, bitmap_size_bytes, MSB_FIRST);
should_ptr(bitmap) not be null;

// El primer bloque debe estar ocupado (para initial_file:BASE)
should_bool(bitarray_test_bit(bitmap, 0)) be truthy;

// Otros bloques deben estar libres
should_bool(bitarray_test_bit(bitmap, 1)) be falsey;
should_bool(bitarray_test_bit(bitmap, total_blocks - 1)) be falsey;

bitarray_destroy(bitmap);
free(bitmap_data);
}
end

            it("retorna error para punto de montaje invalido") {
  int result = init_bitmap("/invalid/path", TEST_FS_SIZE, TEST_BLOCK_SIZE);

  should_int(result) be equal to(-1);
}
end
}
end

    describe("funcion init_blocks_index"){
        it("crea archivo blocks_hash_index.config"){
            int result = init_blocks_index(TEST_MOUNT_POINT);

should_int(result) be equal to(0);

char blocks_path[PATH_MAX];
snprintf(blocks_path, sizeof(blocks_path), "%s/blocks_hash_index.config",
         TEST_MOUNT_POINT);

should_bool(file_exists(blocks_path)) be truthy;
}
end

            it("retorna error para punto de montaje invalido") {
  int result = init_blocks_index("/invalid/path");

  should_int(result) be equal to(-1);
}
end
}
end

    describe("funcion init_physical_blocks"){
        it("crea directorio physical_blocks con todos los archivos de bloques"){
            int result = init_physical_blocks(TEST_MOUNT_POINT, TEST_FS_SIZE,
                                              TEST_BLOCK_SIZE);

should_int(result) be equal to(0);

char physical_blocks_dir[PATH_MAX];
snprintf(physical_blocks_dir, sizeof(physical_blocks_dir), "%s/physical_blocks",
         TEST_MOUNT_POINT);

should_bool(directory_exists(physical_blocks_dir)) be truthy;

int total_blocks = TEST_FS_SIZE / TEST_BLOCK_SIZE;
should_int(count_files_in_directory(physical_blocks_dir)) be equal
    to(total_blocks);

// Verificar que existen los archivos del primer y ultimo bloque con tamaño
// correcto
char first_block[PATH_MAX], last_block[PATH_MAX];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
snprintf(first_block, sizeof(first_block), "%s/block0000.dat", physical_blocks_dir);
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
snprintf(last_block, sizeof(last_block), "%s/block%04d.dat", physical_blocks_dir,
         total_blocks - 1);
#pragma GCC diagnostic pop

should_bool(file_exists(first_block)) be truthy;
should_bool(file_exists(last_block)) be truthy;
should_int(verify_file_size(first_block, TEST_BLOCK_SIZE)) be equal to(1);
should_int(verify_file_size(last_block, TEST_BLOCK_SIZE)) be equal to(1);
}
end

            it("retorna error para punto de montaje invalido") {
  int result =
      init_physical_blocks("/invalid/path", TEST_FS_SIZE, TEST_BLOCK_SIZE);

  should_int(result) be equal to(-1);
}
end
}
end

    describe("funcion init_files"){
        it("crea estructura completa de archivos con hard links"){
            init_physical_blocks(TEST_MOUNT_POINT, TEST_FS_SIZE,
                                 TEST_BLOCK_SIZE);

int result = init_files(TEST_MOUNT_POINT);

should_int(result) be equal to(0);

char files_dir[PATH_MAX];
snprintf(files_dir, sizeof(files_dir), "%s/files", TEST_MOUNT_POINT);
should_bool(directory_exists(files_dir)) be truthy;

char initial_file_dir[PATH_MAX];
snprintf(initial_file_dir, sizeof(initial_file_dir), "%s/files/initial_file",
         TEST_MOUNT_POINT);
should_bool(directory_exists(initial_file_dir)) be truthy;

char base_dir[PATH_MAX];
snprintf(base_dir, sizeof(base_dir), "%s/files/initial_file/BASE",
         TEST_MOUNT_POINT);
should_bool(directory_exists(base_dir)) be truthy;

char logical_blocks_dir[PATH_MAX];
snprintf(logical_blocks_dir, sizeof(logical_blocks_dir),
         "%s/files/initial_file/BASE/logical_blocks", TEST_MOUNT_POINT);
should_bool(directory_exists(logical_blocks_dir)) be truthy;

char metadata_file[PATH_MAX];
snprintf(metadata_file, sizeof(metadata_file),
         "%s/files/initial_file/BASE/metadata.config", TEST_MOUNT_POINT);
should_bool(file_exists(metadata_file)) be truthy;

// Verificar contenido de metadata
char metadata_content[256];
read_file_contents(metadata_file, metadata_content, sizeof(metadata_content));
should_ptr(strstr(metadata_content, "SIZE=0")) not be null;
should_ptr(strstr(metadata_content, "BLOCKS=[0]")) not be null;
should_ptr(strstr(metadata_content, "ESTADO=COMMITTED")) not be null;

// Verificar hardlink
char physical_block[PATH_MAX], logical_block[PATH_MAX];
snprintf(physical_block, sizeof(physical_block), "%s/physical_blocks/block0000.dat",
         TEST_MOUNT_POINT);
snprintf(logical_block, sizeof(logical_block),
         "%s/files/initial_file/BASE/logical_blocks/0000.dat",
         TEST_MOUNT_POINT);

should_int(files_are_hardlinked(physical_block, logical_block)) be equal to(1);
}
end
}
end
}
end
}
