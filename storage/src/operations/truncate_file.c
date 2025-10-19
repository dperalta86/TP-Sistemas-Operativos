#include "truncate_file.h"
#include "../utils/filesystem_utils.h"
#include "globals/globals.h"
#include <commons/config.h>
#include <commons/string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/logger.h>
#include <utils/utils.h>

int maybe_handle_orphaned_physical_block(const char *physical_block_path,
                                         const char *mount_point,
                                         uint32_t query_id) {
  struct stat statbuf;

  if (stat(physical_block_path, &statbuf) != 0) {
    log_error(g_storage_logger,
              "No se pudo obtener el estado del bloque físico %s",
              physical_block_path);
    return -1;
  }

  if (statbuf.st_nlink > 1) {
    log_info(g_storage_logger,
             "El bloque físico %s todavía tiene %lu hard links, no se libera",
             physical_block_path, statbuf.st_nlink);
    return 0;
  }

  const char *filename = strrchr(physical_block_path, '/');
  if (filename == NULL) {
    filename =
        physical_block_path; // Si no hay '/' el path completo es el filename
  } else {
    filename++; // Saltea el '/'
  }

  // Validar que el nombre del archivo siga el formato "blockXXXX.dat"
  int block_number;
  if (sscanf(filename, "block%d.dat", &block_number) != 1) {
    log_error(g_storage_logger, "No se pudo parsear el número de bloque de %s",
              physical_block_path);
    return -2;
  }

  // Unsetear el bit del bloque en el bitmap
  int result = modify_bitmap_bits(mount_point, block_number, 1, 0);
  if (result != 0) {
    log_error(g_storage_logger, "No se pudo liberar el bloque %d en el bitmap",
              block_number);
    return -3;
  }

  log_info(g_storage_logger,
           "##%u - Bloque Físico Liberado - Número de Bloque: %d", query_id,
           block_number);
  log_info(g_storage_logger, "Bloque físico %s liberado exitosamente",
           physical_block_path);
  return 0;
}

int truncate_file(uint32_t query_id, const char *name, const char *tag,
                   int new_size_bytes, const char *mount_point) {
  int retval = 0;

  char storage_config_path[PATH_MAX];
  snprintf(storage_config_path, sizeof(storage_config_path),
           "%s/superblock.config", mount_point);
  t_config *superblock_config = config_create(storage_config_path);
  if (!superblock_config) {
    log_error(g_storage_logger, "No se pudo abrir el config: %s",
              storage_config_path);
    retval = -1;
    goto end;
  }

  int block_size = config_get_int_value(superblock_config, "BLOCK_SIZE");

  char metadata_path[PATH_MAX];
  snprintf(metadata_path, sizeof(metadata_path),
           "%s/files/%s/%s/metadata.config", mount_point, name, tag);
  t_config *metadata_config = config_create(metadata_path);
  if (!metadata_config) {
    log_error(g_storage_logger, "No se pudo abrir el config: %s",
              metadata_path);
    retval = -2;
    goto clean_superblock_config;
  }

  char **old_blocks = config_get_array_value(metadata_config, "BLOCKS");
  int old_block_count = string_array_size(old_blocks);
  int new_block_count =
      (new_size_bytes + block_size - 1) / block_size; // Redondeo hacia arriba

  if (new_block_count == old_block_count) {
    log_info(g_storage_logger,
             "El nuevo tamaño encaja en la misma cantidad de bloques, "
             "no se requiere truncado.");
    goto clean_metadata_config;
  }

  char **new_blocks = string_array_new();
  char target_path[PATH_MAX];
  char resolved_path[PATH_MAX];

  if (new_block_count < old_block_count) {
    // Truncar (liberar bloques sobrantes)
    for (int i = new_block_count; i < old_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%d.bin", mount_point, name, tag,
               i);

      if (access(target_path, F_OK) != 0) {
        log_info(g_storage_logger, "Bloque lógico %d no existía en %s", i,
                 target_path);
        continue;
      }

      if (realpath(target_path, resolved_path) == NULL) {
        log_error(g_storage_logger, "No se pudo resolver el path %s",
                  target_path);
        retval = -3;
        goto clean_new_blocks;
      }

      if (remove(target_path) != 0) {
        log_error(g_storage_logger,
                  "No se pudo eliminar el bloque lógico %d en %s", i,
                  target_path);
        retval = -3;
        goto clean_new_blocks;
      }

      maybe_handle_orphaned_physical_block(resolved_path, mount_point,
                                           query_id);
    }

    for (int i = 0; i < new_block_count; i++) {
      string_array_push(&new_blocks, string_duplicate(old_blocks[i]));
    }
  } else {
    // Expandir (asignar nuevos bloques)
    char physical_block_zero_path[PATH_MAX];
    snprintf(physical_block_zero_path, sizeof(physical_block_zero_path),
             "%s/physical_blocks/block0000.dat", mount_point);

    for (int i = old_block_count; i < new_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%d.bin", mount_point, name, tag,
               i);
      if (link(physical_block_zero_path, target_path) != 0) {
        log_error(g_storage_logger, "No se pudo crear hard link de %s a %s",
                  physical_block_zero_path, target_path);
        retval = -4;
        goto clean_new_blocks;
      }
    }

    for (int i = 0; i < old_block_count; i++)
      string_array_push(&new_blocks, string_duplicate(old_blocks[i]));
    for (int i = old_block_count; i < new_block_count; i++)
      string_array_push(&new_blocks, string_duplicate("0"));
  }

  char *stringified_blocks = get_stringified_array(new_blocks);
  config_set_value(metadata_config, "BLOCKS", stringified_blocks);

  char size_str[32];
  snprintf(size_str, sizeof(size_str), "%d", new_size_bytes);
  config_set_value(metadata_config, "SIZE", size_str);

  config_save(metadata_config);

  log_info(g_storage_logger,
           "## %u - File Truncado: %s:%s - Nuevo Tamaño: %d bytes", query_id,
           name, tag, new_size_bytes);

  free(stringified_blocks);

clean_new_blocks:
  if (new_blocks)
    string_array_destroy(new_blocks);
clean_metadata_config:
  config_destroy(metadata_config);
clean_superblock_config:
  config_destroy(superblock_config);
end:
  return retval;
}

t_package *handle_truncate_file_op_package(t_package *package) {
  uint32_t query_id;
  if (!package_read_uint32(package, &query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de TRUNCATE_FILE");
    return NULL;
  }

  char *name = package_read_string(package);
  char *tag = package_read_string(package);

  uint32_t new_size_bytes;
  if (!package_read_uint32(package, &new_size_bytes)) {
    log_error(g_storage_logger,
              "## Error al deserializar new_size_bytes de TRUNCATE_FILE");
    free(name);
    free(tag);
    return NULL;
  }

  if (!name || !tag) {
    log_error(g_storage_logger,
              "## Error al deserializar parámetros de TRUNCATE_FILE");
    free(name);
    free(tag);
    return NULL;
  }

  int operation_result = truncate_file(query_id, name, tag, new_size_bytes,
                                       g_storage_config->mount_point);

  free(name);
  free(tag);

  t_package *response = package_create_empty(STORAGE_OP_FILE_TRUNCATE_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Error al crear el paquete de respuesta para TRUNCATE_FILE");
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Error al escribir status en respuesta de TRUNCATE_FILE");
    package_destroy(response);
    return NULL;
  }

  return response;
}
