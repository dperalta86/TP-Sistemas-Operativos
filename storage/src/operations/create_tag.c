#include "create_tag.h"
#include "../file_locks.h"
#include <limits.h>

int create_tag(uint32_t query_id, const char *name, const char *src_tag,
               const char *dst_tag) {
  int retval = 0;

  char dst_path[PATH_MAX];
  snprintf(dst_path, PATH_MAX, "%s/files/%s/%s", g_storage_config->mount_point, name, dst_tag);
  lock_file(name, dst_tag, true);

  t_file_metadata *metadata =
      read_file_metadata(g_storage_config->mount_point, name, dst_tag);
  if (metadata) {
    log_error(g_storage_logger,
              "## %u - No se puede crear el tag %s:%s porque ya existe",
              query_id, name, dst_tag);
    retval = FILE_TAG_ALREADY_EXISTS;
    goto end;
  }

  char src_path[PATH_MAX];
  snprintf(src_path, PATH_MAX, "%s/files/%s/%s", g_storage_config->mount_point,
           name, src_tag);

  lock_file(name, src_tag, false);

  char command[PATH_MAX * 2 + 32];
  snprintf(command, sizeof(command), "cp -rl \"%s\" \"%s\"", src_path, dst_path);
  int cmd_status_code = system(command);

  unlock_file(name, src_tag);

  if (cmd_status_code != 0) {
    log_error(g_storage_logger, "## %u - No se pudo copiar de %s:%s a %s:%s",
              query_id, name, src_tag, name, dst_tag);
    retval = -2;
    goto end;
  }

  metadata = read_file_metadata(g_storage_config->mount_point, name, dst_tag);
  if (!metadata) {
    log_error(g_storage_logger,
              "## %u - No se pudo leer metadata después de copiar %s:%s",
              query_id, name, dst_tag);
    retval = -3;
    goto end;
  }

  if (metadata->state) {
    free(metadata->state);
  }
  metadata->state = strdup("WORK_IN_PROGRESS");

  if (save_file_metadata(metadata) != 0) {
    log_error(g_storage_logger,
              "## %u - No se pudo guardar metadata para %s:%s", query_id, name,
              dst_tag);
    retval = -4;
  }

  log_info(g_storage_logger, "## %" PRIu32 " - Tag creado %s:%s", query_id, name, dst_tag);

end:
  unlock_file(name, dst_tag);
  if (metadata) {
    destroy_file_metadata(metadata);
  }
  return retval;
}

t_package *handle_create_tag_op_package(t_package *package) {
  uint32_t query_id;
  if (!package_read_uint32(package, &query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de CREATE_TAG");
    return NULL;
  }

  char *name = package_read_string(package);
  char *src_tag = package_read_string(package);
  char *dst_tag = package_read_string(package);

  if (!name || !src_tag || !dst_tag) {
    log_error(g_storage_logger,
              "## Error al deserializar parámetros de CREATE_TAG");
    if (name)
      free(name);
    if (src_tag)
      free(src_tag);
    if (dst_tag)
      free(dst_tag);
    return NULL;
  }

  int operation_result = create_tag(query_id, name, src_tag, dst_tag);

  free(name);
  free(src_tag);
  free(dst_tag);

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
