#include "truncate_file.h"
#include "../file_locks.h"
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

int delete_logical_block(const char *mount_point, const char *name,
                         const char *tag, int logical_block_index,
                         int physical_block_index, uint32_t query_id) {
  char target_path[PATH_MAX];
  snprintf(target_path, sizeof(target_path),
           "%s/files/%s/%s/logical_blocks/%04d.dat", mount_point, name, tag,
           logical_block_index);

  if (remove(target_path) != 0) {
    log_error(g_storage_logger,
              "No se pudo eliminar el bloque lógico %04d en %s",
              logical_block_index, target_path);
    return -1;
  }

  log_info(g_storage_logger,
           "##%u - Bloque Lógico Eliminado - Nombre: %s, Tag: %s, "
           "Índice: %04d",
           query_id, name, tag, logical_block_index);

  snprintf(target_path, sizeof(target_path), "%s/physical_blocks/block%04d.dat",
           mount_point, physical_block_index);

  struct stat statbuf;
  if (stat(target_path, &statbuf) != 0) {
    log_error(g_storage_logger,
              "No se pudo obtener el estado del bloque físico %04d en %s",
              physical_block_index, target_path);
    return -2;
  }

  if (statbuf.st_nlink != 1) {
    log_info(g_storage_logger,
             "El bloque físico %04d todavía tiene %lu hard links, no se libera",
             physical_block_index, statbuf.st_nlink);
    return 0;
  }

  if (modify_bitmap_bits(mount_point, physical_block_index, 1, 0) != 0) {
    log_error(g_storage_logger,
              "No se pudo liberar el bloque físico %04d en el bitmap",
              physical_block_index);
    return -3;
  }

  log_info(g_storage_logger,
           "##%u - Bloque Físico Liberado - Número de Bloque: %04d", query_id,
           physical_block_index);

  return 0;
}

int truncate_file(uint32_t query_id, const char *name, const char *tag,
                  int new_size_bytes, const char *mount_point) {
  int retval = 0;

  lock_file(name, tag);

  char metadata_path[PATH_MAX];
  snprintf(metadata_path, sizeof(metadata_path),
           "%s/files/%s/%s/metadata.config", mount_point, name, tag);
  t_config *metadata_config = config_create(metadata_path);
  if (!metadata_config) {
    log_error(g_storage_logger, "No se pudo abrir el config: %s",
              metadata_path);
    retval = -1;
    goto unlock_only;
  }

  char **old_blocks = config_get_array_value(metadata_config, "BLOCKS");
  int old_block_count = string_array_size(old_blocks);
  int new_block_count = (new_size_bytes + g_storage_config->block_size - 1) /
                        g_storage_config->block_size; // Redondeo hacia arriba

  if (new_block_count == old_block_count) {
    log_info(g_storage_logger,
             "El nuevo tamaño encaja en la misma cantidad de bloques, "
             "no se requiere truncado.");
    goto clean_metadata_and_old_blocks;
  }

  char **new_blocks = string_array_new();
  char target_path[PATH_MAX];

  if (new_block_count < old_block_count) {
    // Truncar (liberar bloques sobrantes)
    for (int i = new_block_count; i < old_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%04d.dat", mount_point, name, tag,
               i);

      if (access(target_path, F_OK) != 0) {
        log_info(g_storage_logger, "Bloque lógico %04d no existía en %s", i,
                 target_path);
        continue;
      }

      delete_logical_block(mount_point, name, tag, i, atoi(old_blocks[i]),
                           query_id);
    }

    for (int i = 0; i < new_block_count; i++) {
      string_array_push(&new_blocks, string_duplicate(old_blocks[i]));
    }
  } else {
    // Expandir (asignar nuevos bloques)
    char physical_block_zero_path[PATH_MAX];
    snprintf(physical_block_zero_path, sizeof(physical_block_zero_path),
             "%s/physical_blocks/0000.dat", mount_point);

    for (int i = old_block_count; i < new_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%d.dat", mount_point, name, tag,
               i);
      if (link(physical_block_zero_path, target_path) != 0) {
        log_error(g_storage_logger, "No se pudo crear hard link de %s a %s",
                  physical_block_zero_path, target_path);
        retval = -2;
        goto clean_all;
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

clean_all:
  if (new_blocks)
    string_array_destroy(new_blocks);
clean_metadata_and_old_blocks:
  if (old_blocks)
    string_array_destroy(old_blocks);
  config_destroy(metadata_config);
unlock_only:
  unlock_file(name, tag);
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

  if (!name || !tag) {
    log_error(g_storage_logger,
              "## Error al deserializar parámetros de TRUNCATE_FILE");
    free(name);
    free(tag);
    return NULL;
  }

  uint32_t new_size_bytes;
  if (!package_read_uint32(package, &new_size_bytes)) {
    log_error(g_storage_logger,
              "## Error al deserializar new_size_bytes de TRUNCATE_FILE");
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
