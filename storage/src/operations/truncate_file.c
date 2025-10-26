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

int truncate_file(uint32_t query_id, const char *name, const char *tag,
                  int new_size_bytes, const char *mount_point) {
  int retval = 0;

  lock_file(name, tag);
  t_file_metadata *metadata = read_file_metadata(mount_point, name, tag);

  if (!metadata) {
    log_error(g_storage_logger, "No se pudo abrir el archivo de metadata");
    retval = -1;
    goto unlock_only;
  }

  int old_block_count = metadata->block_count;
  int new_block_count = (new_size_bytes + g_storage_config->block_size - 1) /
                        g_storage_config->block_size; // Redondeo hacia arriba

  if (new_block_count == old_block_count) {
    log_info(g_storage_logger,
             "El nuevo tamaño encaja en la misma cantidad de bloques, "
             "no se requiere truncado.");
    goto update_size;
  }

  int *new_blocks = malloc(sizeof(int) * new_block_count);
  if (!new_blocks) {
    log_error(g_storage_logger, "No se pudo asignar memoria para new_blocks");
    retval = -3;
    goto clean_metadata;
  }

  // Copiar solo los bloques existentes (evitar buffer over-read)
  int copy_count =
      (new_block_count < old_block_count) ? new_block_count : old_block_count;
  for (int i = 0; i < copy_count; i++) {
    new_blocks[i] = metadata->blocks[i];
  }

  if (new_block_count < old_block_count) {
    // Truncar (liberar bloques sobrantes)
    for (int i = new_block_count; i < old_block_count; i++) {
      delete_logical_block(mount_point, name, tag, i, metadata->blocks[i],
                           query_id);
    }
  } else {
    // Expandir (asignar nuevos bloques)
    char target_path[PATH_MAX];
    char physical_block_zero_path[PATH_MAX];
    snprintf(physical_block_zero_path, sizeof(physical_block_zero_path),
             "%s/physical_blocks/0000.dat", mount_point);

    for (int i = old_block_count; i < new_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%04d.dat", mount_point, name, tag,
               i);
      if (link(physical_block_zero_path, target_path) != 0) {
        log_error(g_storage_logger, "No se pudo crear hard link de %s a %s",
                  physical_block_zero_path, target_path);
        free(new_blocks);
        retval = -2;
        goto clean_metadata;
      }
      new_blocks[i] = 0;
    }
  }

  if (metadata->blocks) {
    free(metadata->blocks);
  }
  metadata->blocks = new_blocks;
  metadata->block_count = new_block_count;

update_size:
  metadata->size = new_size_bytes;

  if (save_file_metadata(metadata) != 0) {
    log_error(g_storage_logger, "No se pudo guardar metadata");
    retval = -4;
    goto clean_metadata;
  }

  log_info(g_storage_logger,
           "## %u - File Truncado: %s:%s - Nuevo Tamaño: %d bytes", query_id,
           name, tag, new_size_bytes);

clean_metadata:
  destroy_file_metadata(metadata);
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
