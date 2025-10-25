#include "fresh_start.h"
#include "../utils/filesystem_utils.h"

/**
 * Borra todo el contenido del directorio de montaje excepto superblock.config
 *
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @return 0 en caso de exito, -1 si se rompe
 */
int wipe_storage_content(const char *mount_point) {
  struct stat st;
  if (stat(mount_point, &st) != 0 || !S_ISDIR(st.st_mode)) {
    log_error(g_storage_logger,
              "El directorio %s no existe o no es un directorio", mount_point);
    return -1;
  }

  // Comando que borra todo excepto superblock.config
  char command[PATH_MAX + 50];
  snprintf(command, sizeof(command),
           "find %s -mindepth 1 ! -name 'superblock.config' -delete",
           mount_point);

  if (system(command) != 0) {
    log_error(g_storage_logger,
              "No se pudo limpiar el contenido del directorio %s", mount_point);
    return -1;
  }

  log_info(
      g_storage_logger,
      "Contenido del directorio %s limpiado (preservando superblock.config)",
      mount_point);
  return 0;
}

/**
 * Crea el bitmap que dice qué bloques están libres o ocupados
 *
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param fs_size Tamaño total del filesystem
 * @param block_size Tamaño de cada bloque
 * @return 0 en caso de exito, -1 si no puede abrir el archivo, -2 si no hay
 * memoria
 */
int init_bitmap(const char *mount_point, int fs_size, int block_size) {
  int retval = 0;

  // Checkear si el mount point existe
  struct stat st;
  if (stat(mount_point, &st) != 0 || !S_ISDIR(st.st_mode)) {
    log_error(g_storage_logger,
              "El directorio %s no existe o no es un directorio", mount_point);
    return -1;
  }

  char bitmap_path[PATH_MAX];
  snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin", mount_point);

  int total_blocks = fs_size / block_size;
  size_t bitmap_size_bytes =
      (total_blocks + 7) / 8; // Redondear al próximo byte

  // Setear todo el buffer a cero (todos los bloques libres)
  char *zero_buffer = calloc(1, bitmap_size_bytes);
  if (zero_buffer == NULL) {
    log_error(g_storage_logger, "No se pudo asignar memoria para el bitmap");
    return -2;
  }

  t_bitarray *bitmap =
      bitarray_create_with_mode(zero_buffer, bitmap_size_bytes, MSB_FIRST);
  if (bitmap == NULL) {
    log_error(g_storage_logger, "No se pudo crear el bitmap en memoria");
    retval = -3;
    goto clean_buffer;
  }

  // El primer bloque siempre está ocupado por el archivo inicial
  bitarray_set_bit(bitmap, 0);

  FILE *bitmap_file = fopen(bitmap_path, "wb");
  if (bitmap_file == NULL) {
    log_error(g_storage_logger, "No se pudo crear el archivo %s", bitmap_path);
    retval = -4;
    goto clean_bitmap;
  }

  fwrite(zero_buffer, 1, bitmap_size_bytes, bitmap_file);
  log_info(g_storage_logger, "Bitmap inicializado en %s con %d bloques",
           bitmap_path, total_blocks);

  fclose(bitmap_file);
clean_bitmap:
  bitarray_destroy(bitmap);
clean_buffer:
  free(zero_buffer);

  return retval;
}

/**
 * Arma el archivo de hashes de bloques para no tener bloques duplicados
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de exito, -1 si se rompe
 */
int init_blocks_index(const char *mount_point) {
  char blocks_path[PATH_MAX];
  snprintf(blocks_path, sizeof(blocks_path), "%s/blocks_hash_index.config",
           mount_point);

  FILE *blocks_ptr = fopen(blocks_path, "wb");
  if (blocks_ptr == NULL) {
    log_error(g_storage_logger, "No se pudo crear el archivo %s", blocks_path);
    return -1;
  }

  fclose(blocks_ptr);

  log_info(g_storage_logger, "Índice de bloques creado en %s", blocks_path);
  return 0;
}

/**
 * Crea el directorio de bloques físicos y todos los archivos de bloques
 *
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param fs_size Tamaño total del filesystem
 * @param block_size Tamaño de cada bloque
 * @return 0 en caso de exito, -1 si no puede crear el directorio, -2 si fallan
 * los bloques, -3 si no hay memoria
 */
int init_physical_blocks(const char *mount_point, int fs_size, int block_size) {
  char physical_blocks_dir_path[PATH_MAX];
  snprintf(physical_blocks_dir_path, sizeof(physical_blocks_dir_path),
           "%s/physical_blocks", mount_point);

  if (create_dir_recursive(physical_blocks_dir_path) != 0) {
    return -1;
  }

  int total_blocks = fs_size / block_size;
  char new_block_path[PATH_MAX];
  void *zero_buffer = calloc(1, block_size);
  if (zero_buffer == NULL) {
    log_error(g_storage_logger, "No se pudo asignar memoria para los bloques");
    return -3;
  }

  for (int i = 0; i < total_blocks; i++) {
// Desactiva el warning de GCC sobre truncamiento de snprintf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(new_block_path, sizeof(new_block_path), "%s/%04d.dat",
             physical_blocks_dir_path, i);
#pragma GCC diagnostic pop

    FILE *block_ptr = fopen(new_block_path, "wb");
    if (block_ptr == NULL) {
      log_error(g_storage_logger, "No se pudo crear el archivo de bloque %s",
                new_block_path);
      free(zero_buffer);
      return -2;
    }

    fwrite(zero_buffer, 1, block_size, block_ptr);

    fclose(block_ptr);
  }

  free(zero_buffer);

  log_info(g_storage_logger, "Creados %d bloques físicos en %s", total_blocks,
           physical_blocks_dir_path);
  return 0;
}

/**
 * Arma toda la estructura de archivos con el archivo inicial y sus metadatos
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de exito, números negativos (-1 a -6) si se rompe algo
 */
int init_files(const char *mount_point) {
  char source_path[PATH_MAX];
  char target_path[PATH_MAX];

  // Crear estructura de carpetas para el archivo inicial
  if (create_file_dir_structure(mount_point, "initial_file", "BASE") != 0) {
    log_error(g_storage_logger,
              "No se pudo crear la estructura de carpetas para initial_file");
    return -1;
  }

  snprintf(source_path, PATH_MAX, "%s/physical_blocks/0000.dat", mount_point);
  snprintf(target_path, PATH_MAX,
           "%s/files/initial_file/BASE/logical_blocks/0000.dat", mount_point);
  if (link(source_path, target_path) != 0) {
    log_error(g_storage_logger, "No se pudo crear el hard link %s -> %s",
              source_path, target_path);
    return -5;
  }

  if (create_metadata_file(mount_point, "initial_file", "BASE",
                           "SIZE=0\nBLOCKS=[0]\nESTADO=COMMITTED\n") != 0) {
    return -6;
  }

  log_info(g_storage_logger, "Archivo inicial creado");

  return 0;
}

/**
 * Monta el filesystem completo ejecutando todas las funciones de inicialización
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de exito, números negativos (-1 a -6) que te dicen qué
 * función se rompió
 */
int init_storage(const char *mount_point) {
  int fs_size, block_size;

  if (read_superblock(mount_point, &fs_size, &block_size) != 0)
    return -1;
  if (wipe_storage_content(mount_point) != 0)
    return -2;
  if (init_bitmap(mount_point, fs_size, block_size) != 0)
    return -3;
  if (init_blocks_index(mount_point) != 0)
    return -4;
  if (init_physical_blocks(mount_point, fs_size, block_size) != 0)
    return -5;
  if (init_files(mount_point) != 0)
    return -6;

  return 0;
}
