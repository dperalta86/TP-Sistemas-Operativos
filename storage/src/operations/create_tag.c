#include "create_tag.h"
#include "../file_locks.h"
#include "error_messages.h"
#include <limits.h>

int create_tag(uint32_t query_id, const char *file_src, const char *tag_src,
               const char *file_dst, const char *tag_dst) {
  int retval = 0;

  char tag_dst_dir[PATH_MAX]; 
  snprintf(tag_dst_dir, PATH_MAX, "%s/files/%s/%s", g_storage_config->mount_point,
           file_dst, tag_dst);

  char dst_path[PATH_MAX];
  snprintf(dst_path, PATH_MAX, "%s/files/%s/%s/logical_blocks", g_storage_config->mount_point,
           file_dst, tag_dst);

  //lock_file(file_dst, tag_dst, true);

  t_file_metadata *metadata_dst =
      read_file_metadata(g_storage_config->mount_point, file_dst, tag_dst);
  if (metadata_dst) {
    log_error(g_storage_logger,
              "## %u - No se puede crear el tag %s:%s porque ya existe",
              query_id, file_dst, tag_dst);
    retval = FILE_TAG_ALREADY_EXISTS;
    goto end;
  }

  //lock_file(file_src, tag_src, false);

  t_file_metadata *metadata_src =
      read_file_metadata(g_storage_config->mount_point, file_src, tag_src);
  if (metadata_src == NULL) {
    log_error(g_storage_logger,
              "## %u - El tag de origen %s:%s no existe.",
              query_id, file_src, tag_src);
    retval = FILE_TAG_MISSING;
    goto cleanup_source_lock;
  }
  
  log_debug(g_storage_logger, "## %u - Fuente Tag %s:%s encontrada. Intentando crear destino.",
           query_id, file_src, tag_src);

  char src_path[PATH_MAX];
  snprintf(src_path, PATH_MAX, "%s/files/%s/%s/logical_blocks", g_storage_config->mount_point,
           file_src, tag_src);
             
  // Crear el directorio del Tag destino 
  if (mkdir(tag_dst_dir, 0777) != 0) {
      if (errno != EEXIST) {
          log_error(g_storage_logger, "## %u - Fallo al crear directorio Tag: %s. Error: %s",
                    query_id, tag_dst_dir, strerror(errno));
          retval = -1;
          goto cleanup_source_lock;
      }
  }
  
  // Crear el directorio de Bloques Lógicos destino
  if (mkdir(dst_path, 0777) != 0) {
      if (errno != EEXIST) {
          log_error(g_storage_logger, "## %u - Fallo al crear directorio logical_blocks: %s. Error: %s",
                    query_id, dst_path, strerror(errno));
          retval = -2;
          goto cleanup_source_lock;
      }
  }

  char command[PATH_MAX * 2 + 32];
  snprintf(command, sizeof(command), "cp -rl \"%s/.\" \"%s\"", src_path, dst_path);
  
  log_debug(g_storage_logger, "## %u - Ejecutando comando de copia: %s", query_id, command);
  
  int cmd_status_code = system(command);

  log_debug(g_storage_logger, "## %u - Comando 'cp' terminado. Código de retorno: %d",
           query_id, cmd_status_code);

  if (cmd_status_code != 0) {
    log_error(g_storage_logger, "## %u - Fallo en 'cp' al copiar hardlinks. Código: %d",
              query_id, cmd_status_code);
    retval = -3;
    goto cleanup_source_lock;
  }
  
  char *block_array_str = config_get_string_value(metadata_src->config, "BLOCKS");
  
  if(create_metadata(file_dst, tag_dst, metadata_src->size, metadata_src->block_count, block_array_str,
                 "WORK_IN_PROGRESS", g_storage_config->mount_point) < 0) {
    log_error(g_storage_logger, "## %u - No se pudo crear el metadata para %s:%s",
              query_id, file_dst, tag_dst);
    retval = -3;
    goto cleanup_source_lock;
  }
                 
  log_info(g_storage_logger, "## %" PRIu32 " - Tag creado %s:%s", query_id,
           file_dst, tag_dst);

cleanup_source_lock:
  //unlock_file(file_src, tag_src);
  if (metadata_src)
    destroy_file_metadata(metadata_src);

end:
  //unlock_file(file_dst, tag_dst);
  if (metadata_dst) 
    destroy_file_metadata(metadata_dst);
  return retval;
}

t_package *handle_create_tag_op_package(t_package *package) {
  uint32_t query_id;
  if (!package_read_uint32(package, &query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de CREATE_TAG");
    return NULL;
  }

  char *file_src = package_read_string(package);
  char *tag_src = package_read_string(package);
  char *file_dst = package_read_string(package);
  char *tag_dst = package_read_string(package);

  if (!file_src || !tag_src || !file_dst || !tag_dst) {
    log_error(g_storage_logger,
              "## Error al deserializar parámetros de CREATE_TAG");
    if (file_src)
      free(file_src);
    if (tag_src)
      free(tag_src);
    if (file_dst)
      free(file_dst);
    if (tag_dst)
      free(tag_dst);
    return NULL;
  }

  int operation_result =
      create_tag(query_id, file_src, tag_src, file_dst, tag_dst);

  free(file_src);
  free(tag_src);
  free(file_dst);
  free(tag_dst);

  if (operation_result != 0) {
    char *error_message = string_from_format("CREATE_TAG error: %s", storage_error_message(operation_result));
    t_package *response = package_create_empty(STORAGE_OP_ERROR);
    if (!response) {
      log_error(g_storage_logger,
                "## Error al crear el paquete de error para CREATE_TAG");
      free(error_message);
      return NULL;
    }
    package_add_uint32(response, query_id);
    package_add_string(response, error_message);
    free(error_message);
    package_reset_read_offset(response);
    return response;
  }

  t_package *response = package_create_empty(STORAGE_OP_TAG_CREATE_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Error al crear el paquete de respuesta para CREATE_TAG");
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Error al escribir status en respuesta de CREATE_TAG");
    package_destroy(response);
    return NULL;
  }

  package_reset_read_offset(response);

  return response;
}

int create_metadata(const char *name, const char *tag, int size, int numb_blocks, char *blocks_array_str, char *status, char *mount_point) {
    int retval = 0;

    char **blocks_array = string_get_string_as_array(blocks_array_str);
    if(string_array_size(blocks_array) != numb_blocks) {
        retval = -1;
        goto cleanup_array;
    }

    char metadata_path[PATH_MAX];
    snprintf(metadata_path, sizeof(metadata_path), "%s/files/%s/%s/metadata.config", mount_point, name, tag);
    FILE* metadata_file = fopen(metadata_path, "w");
    if (metadata_file == NULL) {
        retval = -2;
        goto cleanup_array;
    }

    fprintf(metadata_file, "SIZE=%d\nBLOCKS=%s\nESTADO=%s\n", size, blocks_array_str, status);
    fclose(metadata_file);

cleanup_array:
    string_array_destroy(blocks_array);
    return retval;
}