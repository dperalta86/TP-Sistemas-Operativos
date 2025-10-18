#include "create_file.h"
#include "utils/filesystem_utils.h"
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <utils/logger.h>

int _create_file(uint32_t query_id, const char *name, const char *tag,
                 const char *mount_point) {

  int result = create_file_dir_structure(mount_point, name, tag);
  if (result != 0) {
    log_error(g_storage_logger, "Error al crear el archivo %s con tag %s", name,
              tag);
    return result;
  }

  if (create_metadata_file(mount_point, name, tag, NULL) != 0) {
    log_error(g_storage_logger,
              "Error al crear el metadata del archivo %s con tag %s", name,
              tag);
    delete_file_dir_structure(mount_point, name, tag);

    return -2;
  }

  log_info(g_storage_logger, "## %u - File Creado: %s:%s", query_id, name, tag);

  return 0;
}

t_package *create_file(t_package *package) {
  uint32_t query_id;
  if (!package_read_uint32(package, &query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de CREATE_FILE");
    return NULL;
  }

  char *name = package_read_string(package);
  char *tag = package_read_string(package);
  if (!name || !tag) {
    log_error(g_storage_logger,
              "## Error al deserializar parámetros de CREATE_FILE");
    free(name);
    free(tag);
    return NULL;
  }

  int operation_result =
      _create_file(query_id, name, tag, g_storage_config->mount_point);

  free(name);
  free(tag);

  t_package *response = package_create_empty(STORAGE_OP_FILE_CREATE_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Error al crear el paquete de respuesta para CREATE_FILE");
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Error al escribir status en respuesta de CREATE_FILE");
    package_destroy(response);
    return NULL;
  }

  return response;
}
