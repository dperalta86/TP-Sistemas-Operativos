#include "storage_utils.h"
#include "globals/globals.h"
#include <commons/bitarray.h>
#include <commons/string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

/**
 * Lee el archivo superblock.config y obtiene la configuración del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param fs_size Puntero donde se almacenará el tamaño del filesystem
 * @param block_size Puntero donde se almacenará el tamaño de los bloques
 * @return 0 en caso de éxito, -1 si no puede abrir el archivo, -2 si faltan
 * propiedades
 */
int read_superblock(const char *mount_point, int *fs_size, int *block_size) {
  char superblock_path[PATH_MAX];
  snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config",
           mount_point);

  t_config *config = config_create(superblock_path);
  if (config == NULL) {
    log_error(g_storage_logger, "No se pudo abrir el archivo %s",
              superblock_path);
    return -1;
  }

  if (!config_has_property(config, "FS_SIZE") ||
      !config_has_property(config, "BLOCK_SIZE")) {
    log_error(g_storage_logger, "El superblock no tiene las propiedades "
                                "requeridas (FS_SIZE, BLOCK_SIZE)");
    config_destroy(config);
    return -2;
  }

  *fs_size = config_get_int_value(config, "FS_SIZE");
  *block_size = config_get_int_value(config, "BLOCK_SIZE");

  config_destroy(config);

  log_info(g_storage_logger, "Superblock leído: FS_SIZE=%d, BLOCK_SIZE=%d",
           *fs_size, *block_size);
  return 0;
}

/**
 * Calcula el tamaño en bytes necesario para el bitmap del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return Tamaño en bytes del bitmap, -1 si hay error
 */
size_t get_bitmap_size_bytes(const char *mount_point) {
  int fs_size, block_size;

  if (read_superblock(mount_point, &fs_size, &block_size) != 0) {
    return (size_t)-1;
  }

  int total_blocks = fs_size / block_size;
  size_t bitmap_size_bytes =
      (total_blocks + 7) / 8; // Redondear al próximo byte

  return bitmap_size_bytes;
}

/**
 * Modifica múltiples bits en el bitmap del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param indexes Array de índices de bloques a modificar
 * @param count Cantidad de elementos en el array indexes
 * @param set_bits 1 para setear bits (marcar como ocupados), 0 para unsetear
 * (marcar como libres)
 * @return 0 en caso de éxito, -1 si hay error abriendo bitmap, -2 si hay error
 * de memoria, -3 si hay error escribiendo
 */
int modify_bitmap_bits(const char *mount_point, int *indexes, size_t count,
                       int set_bits) {
  int retval = 0;
  FILE *bitmap_file = NULL;
  char *bitmap_buffer = NULL;
  t_bitarray *bitmap = NULL;

  // Obtener el tamaño del bitmap
  size_t bitmap_size_bytes = get_bitmap_size_bytes(mount_point);
  if (bitmap_size_bytes == (size_t)-1) {
    log_error(g_storage_logger, "No se pudo calcular el tamaño del bitmap");
    retval = -1;
    goto end;
  }

  // Abrir el archivo bitmap
  char bitmap_path[PATH_MAX];
  snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin", mount_point);

  bitmap_file = fopen(bitmap_path, "r+b");
  if (!bitmap_file) {
    log_error(g_storage_logger, "No se pudo abrir el archivo bitmap: %s",
              bitmap_path);
    retval = -1;
    goto end;
  }

  // Leer el bitmap existente
  bitmap_buffer = calloc(1, bitmap_size_bytes);
  if (!bitmap_buffer) {
    log_error(g_storage_logger, "No se pudo asignar memoria para el bitmap");
    retval = -2;
    goto clean_file;
  }

  if (fread(bitmap_buffer, 1, bitmap_size_bytes, bitmap_file) !=
      bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo leer el bitmap completo");
    retval = -1;
    goto clean_buffer;
  }

  bitmap =
      bitarray_create_with_mode(bitmap_buffer, bitmap_size_bytes, MSB_FIRST);
  if (!bitmap) {
    log_error(g_storage_logger, "No se pudo crear el bitmap en memoria");
    retval = -2;
    goto clean_buffer;
  }

  // Modificar los bits según el parámetro set_bits
  for (size_t i = 0; i < count; i++) {
    if (set_bits) {
      bitarray_set_bit(bitmap, indexes[i]);
    } else {
      bitarray_clean_bit(bitmap, indexes[i]);
    }
  }

  // Escribir el bitmap modificado de vuelta al archivo
  fseek(bitmap_file, 0, SEEK_SET);
  if (fwrite(bitmap_buffer, 1, bitmap_size_bytes, bitmap_file) !=
      bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo escribir el bitmap modificado");
    retval = -3;
    goto clean_bitmap;
  }

  log_info(g_storage_logger, "Modificados %zu bits en el bitmap (%s)", count,
           set_bits ? "seteados" : "unseteados");

clean_bitmap:
  if (bitmap)
    bitarray_destroy(bitmap);
clean_buffer:
  if (bitmap_buffer)
    free(bitmap_buffer);
clean_file:
  if (bitmap_file)
    fclose(bitmap_file);
end:
  return retval;
}
