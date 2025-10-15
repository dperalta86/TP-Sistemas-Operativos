#include "operations/create_file.h"
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <utils/logger.h>

int _create_file(uint32_t query_id, const char *name, const char *tag,
                 const char *mount_point) {
  char target_path[PATH_MAX];

  // Crear carpeta del File
  snprintf(target_path, sizeof(target_path), "%s/files/%s", mount_point, name);
  mkdir(target_path, 0755); // Ignoramos el error si ya existe

  // Crear carpeta del Tag
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point,
           name, tag);
  if (mkdir(target_path, 0755) != 0)
    goto file_creation_error;

  // Crear carpeta de bloques lógicos
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/logical_blocks",
           mount_point, name, tag);
  if (mkdir(target_path, 0755) != 0)
    goto file_creation_error;

  // Crear archivo de metadata
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/metadata.config",
           mount_point, name, tag);
  FILE *metadata_ptr = fopen(target_path, "w");
  if (metadata_ptr == NULL) {
    log_error(g_storage_logger, "No se pudo crear el archivo %s", target_path);
    return -2;
  }

  fprintf(metadata_ptr, "SIZE=0\nBLOCKS=[]\nESTADO=WORK_IN_PROGRESS\n");
  fclose(metadata_ptr);

  log_info(g_storage_logger, "## %u - File Creado: %s:%s", query_id, name, tag);

  return 0;

file_creation_error:
  log_error(g_storage_logger, "Error al crear el archivo %s con tag %s", name, tag);
  return -1;
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

  int operation_result = _create_file(
      query_id, name, tag, g_storage_config->mount_point);

  free(name);
  free(tag);

  t_package *response = package_create_empty(STORAGE_OP_FILE_CREATE_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Error al crear el paquete de respuesta para CREATE_FILE");
    return NULL;
  }

  if (!package_add_uint8(response, operation_result)) {
    log_error(g_storage_logger,
              "## Error al escribir status en respuesta de CREATE_FILE");
    package_destroy(response);
    return NULL;
  }

  return response;
}
