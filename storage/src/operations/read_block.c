#include "read_block.h"
#include "error_messages.h"

t_package *handle_read_block_request(t_package *package) {
  uint32_t query_id;
  char *name = NULL;
  char *tag = NULL;
  uint32_t block_number;

  if (deserialize_block_read_request(package, &query_id, &name, &tag, &block_number) < 0) {
    return NULL;
  }

  void *read_buffer = malloc(g_storage_config->block_size + 1);
    if (!read_buffer) {
        log_error(g_storage_logger, "## Query ID: %" PRIu32 " - Fallo al asignar memoria para lectura del bloque %" PRIu32 ".", query_id, block_number);
        free(name);
        free(tag);
        return NULL;
  }  

  int operation_result = execute_block_read(name, tag, query_id, block_number, read_buffer);

  free(name);
  free(tag);

  if (operation_result != 0) {
    char *error_message = string_from_format("READ_BLOCK error: %s", storage_error_message(operation_result));
    t_package *response = package_create_empty(STORAGE_OP_ERROR);
    if (!response) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32 " - Fallo al crear paquete de error.",
                query_id);
      free(read_buffer);
      free(error_message);
      return NULL;
    }
    package_add_uint32(response, query_id);
    package_add_string(response, error_message);
    free(error_message);
    package_reset_read_offset(response);
    free(read_buffer);
    return response;
  }

  t_package *response = package_create_empty(STORAGE_OP_BLOCK_READ_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - Fallo al crear paquete de respuesta.",
              query_id);
    free(read_buffer);
    return NULL;
  }

  uint32_t data_size_to_send = (uint32_t) g_storage_config->block_size;

  if (!package_add_uint32(response, data_size_to_send)) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - Error al escribir tamaño en respuesta de READ BLOCK", query_id);
    package_destroy(response);
    free(read_buffer);
    return NULL;
  }

  if (data_size_to_send > 0) {
    if (!package_add_data(response, read_buffer, data_size_to_send)) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32 " - Error al escribir contenido binario del bloque en respuesta.", query_id);
      package_destroy(response);
      free(read_buffer);
      return NULL;
    }
  }

  free(read_buffer);
  package_reset_read_offset(response);

  return response;
}

int deserialize_block_read_request(t_package *package, uint32_t *query_id, char **name, char **tag, uint32_t *block_number) {
  int retval = 0;

  if (!package_read_uint32(package, query_id)) {
    log_error(g_storage_logger, "## Error al deserializar query_id de READ_BLOCK");
    retval = -1;
    goto end;
  }

  *name = package_read_string(package);
  if (*name == NULL) {
    log_error(g_storage_logger, "## Query ID: %" PRIu32 " - Error al deserializar el nombre del file de READ_BLOCK", *query_id);
    retval = -1;
    goto end;
  }

  *tag = package_read_string(package);
  if (*tag == NULL) {
    log_error(g_storage_logger, "## Query ID: %" PRIu32 " - Error al deserializar el tag del file de READ_BLOCK", *query_id);
    retval = -1;
    goto clean_name;
  }

  if (!package_read_uint32(package, block_number)) {
    log_error(g_storage_logger, "## Query ID: %" PRIu32 " - Error al deserializar el número bloque de READ_BLOCK", *query_id);
    retval = -1;
    goto clean_tag;
  }

  return retval;

clean_tag:
  if (*tag) {
    free(*tag);
    *tag = NULL;
  }
clean_name:
  if (*name) {
    free(*name);
    *name = NULL;
  }
end:
  return retval;
}

int execute_block_read(const char *name, const char *tag, uint32_t query_id,
                        uint32_t block_number, void *read_buffer) {
  int retval = 0;

  lock_file(name, tag, false);
  log_debug(g_storage_logger, "/**** Query ID %" PRIu32 ": Lock de lectura adquirido.", query_id);

  if (!file_dir_exists(name, tag)) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - El directorio %s:%s no existe.", query_id,
              name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  t_file_metadata *metadata =
      read_file_metadata(g_storage_config->mount_point, name, tag);
  if (metadata == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - No se pudo leer el metadata de %s:%s.",
              query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  if ((int)block_number >= metadata->block_count) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - El bloque lógico %" PRIu32 " no existe en %s:%s. Fuera "
              "de rango [0, %d]",
              query_id, block_number, name, tag, metadata->block_count);
    retval = READ_OUT_OF_BOUNDS;
    goto cleanup_metadata;
  }

  destroy_file_metadata(metadata);

  if (read_from_logical_block(query_id, name, tag, block_number, read_buffer) < 0) {
    retval = -1;
  }

  goto cleanup_unlock;

cleanup_metadata:
  if (metadata)
    destroy_file_metadata(metadata);
cleanup_unlock:
  unlock_file(name, tag);
  log_debug(g_storage_logger, "/**** Query ID %" PRIu32 ": Lock de lectura liberado.", query_id);
  usleep(g_storage_config->block_access_delay/2 * 1000);

  return retval;
}

int read_from_logical_block(uint32_t query_id, const char *file_name,
                           const char *tag, uint32_t block_number, void *read_buffer) {
  int retval = 0;

  char logical_block_path[PATH_MAX];
  snprintf(logical_block_path, sizeof(logical_block_path),
           "%s/files/%s/%s/logical_blocks/%04" PRIu32 ".dat",
           g_storage_config->mount_point, file_name, tag, block_number);

  FILE *block_file = fopen(logical_block_path, "rb");
  if (block_file == NULL) {
    log_error(g_storage_logger, "## Query ID: %" PRIu32 " - No se pudo abrir el bloque %s para lectura.",
              query_id, logical_block_path);
    return -1;
  }

  usleep(g_storage_config->block_access_delay/2 * 1000);
          
  sched_yield();

  size_t bytes_leidos = fread(read_buffer, 1, g_storage_config->block_size, block_file);

  if (bytes_leidos != g_storage_config->block_size) {
    if (ferror(block_file)) {
      log_error(g_storage_logger, "## Query ID: %" PRIu32 " - Error de lectura en el bloque: %s.",
                query_id, logical_block_path);
      retval = -2;
    } else {
      log_error(g_storage_logger, "## Query ID: %" PRIu32 " - Lectura parcial o EOF inesperado.",
                query_id);
      retval = -3;
    }
  } else {
    ((char*)read_buffer)[g_storage_config->block_size] = '\0';
  }

  log_info(g_storage_logger, "## Query ID: %" PRIu32 " - Bloque lógico leído %s:%s - Número de bloque: %" PRIu32, 
           query_id, file_name, tag, block_number);

  fclose(block_file);
  return retval;
}