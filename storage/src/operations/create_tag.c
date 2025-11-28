#include "create_tag.h"
#include "../file_locks.h"
#include <limits.h>

int create_tag(uint32_t query_id, const char *file_src, const char *tag_src,
               const char *file_dst, const char *tag_dst) {
  int retval = 0;

  char dst_path[PATH_MAX];
  snprintf(dst_path, PATH_MAX, "%s/files/%s/%s", g_storage_config->mount_point,
           file_dst, tag_dst);
  lock_file(file_dst, tag_dst, true);

  t_file_metadata *metadata =
      read_file_metadata(g_storage_config->mount_point, file_dst, tag_dst);
  if (metadata) {
    log_error(g_storage_logger,
              "## %u - No se puede crear el tag %s:%s porque ya existe",
              query_id, file_dst, tag_dst);
    retval = FILE_TAG_ALREADY_EXISTS;
    goto end;
  }

  lock_file(file_src, tag_src, false);

  t_file_metadata *metadata_src =
      read_file_metadata(g_storage_config->mount_point, file_src, tag_src);
  if (metadata_src == NULL) {
    log_error(g_storage_logger,
              "## %u - El tag de origen %s:%s no existe.",
              query_id, file_src, tag_src);
    retval = FILE_TAG_MISSING;
    goto end;
  }

  destroy_file_metadata(metadata_src);

  char src_path[PATH_MAX];
  snprintf(src_path, PATH_MAX, "%s/files/%s/%s", g_storage_config->mount_point,
           file_src, tag_src);

  char command[PATH_MAX * 2 + 32];
  snprintf(command, sizeof(command), "cp -rl \"%s\" \"%s\"", src_path,
           dst_path);
  int cmd_status_code = system(command);

  unlock_file(file_src, tag_src);

  if (cmd_status_code != 0) {
    log_error(g_storage_logger, "## %u - No se pudo copiar de %s:%s a %s:%s",
              query_id, file_src, tag_src, file_dst, tag_dst);
    retval = -2;
    goto end;
  }

  metadata =
      read_file_metadata(g_storage_config->mount_point, file_dst, tag_dst);
  if (!metadata) {
    log_error(g_storage_logger,
              "## %u - No se pudo leer metadata después de copiar %s:%s",
              query_id, file_dst, tag_dst);
    retval = -3;
    goto end;
  }

  if (metadata->state) {
    free(metadata->state);
  }
  metadata->state = strdup("WORK_IN_PROGRESS");

  if (save_file_metadata(metadata) != 0) {
    log_error(g_storage_logger,
              "## %u - No se pudo guardar metadata para %s:%s", query_id,
              file_dst, tag_dst);
    retval = -4;
  }

  log_info(g_storage_logger, "## %" PRIu32 " - Tag creado %s:%s", query_id,
           file_dst, tag_dst);

end:
  unlock_file(file_dst, tag_dst);
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
