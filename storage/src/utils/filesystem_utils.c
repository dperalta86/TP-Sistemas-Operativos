#include "filesystem_utils.h"
#include "../errors.h"
#include "../globals/globals.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <utils/logger.h>

int create_dir_recursive(const char *path) {
  char command[PATH_MAX + 20];
  snprintf(command, sizeof(command), "mkdir -p \"%s\"", path);

  if (system(command) != 0) {
    log_error(g_storage_logger, "No se pudo crear la carpeta %s", path);
    return -1;
  }
  return 0;
}

int create_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag) {
  char target_path[PATH_MAX];
  struct stat st;

  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point,
           file_name, tag);
  if (stat(target_path, &st) == 0) {
    log_error(g_storage_logger,
              "El tag %s ya existe para el archivo %s, reportando "
              "FILE_TAG_ALREADY_EXISTS",
              tag, file_name);
    return FILE_TAG_ALREADY_EXISTS;
  }

  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/logical_blocks",
           mount_point, file_name, tag);

  return create_dir_recursive(target_path);
}

int delete_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag) {
  char target_path[PATH_MAX];
  struct stat st;
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point,
           file_name, tag);

  if (stat(target_path, &st) != 0) {
    log_warning(g_storage_logger,
                "La carpeta %s no existe, reportando FILE_TAG_MISSING",
                target_path);
    return FILE_TAG_MISSING;
  }

  char command[PATH_MAX + 20];
  snprintf(command, sizeof(command), "rm -rf \"%s\"", target_path);
  if (system(command) != 0) {
    log_error(g_storage_logger, "No se pudo eliminar la carpeta %s",
              target_path);
    return -1;
  }

  return 0;
}

int create_metadata_file(const char *mount_point, const char *file_name,
                         const char *tag, const char *initial_content) {
  char metadata_path[PATH_MAX];

  snprintf(metadata_path, sizeof(metadata_path),
           "%s/files/%s/%s/metadata.config", mount_point, file_name, tag);

  FILE *metadata_ptr = fopen(metadata_path, "w");
  if (metadata_ptr == NULL) {
    log_error(g_storage_logger, "No se pudo crear el archivo %s",
              metadata_path);
    return -1;
  }

  const char *content = initial_content
                            ? initial_content
                            : "SIZE=0\nBLOCKS=[]\nESTADO=WORK_IN_PROGRESS\n";
  fprintf(metadata_ptr, "%s", content);
  fclose(metadata_ptr);

  return 0;
}

// TODO: Aplicar en otras funciones
int file_dir_exists(const char *mount_point, const char*file_name, const char *tag) {
  char tag_path[PATH_MAX];
  struct stat st;

  snprintf(tag_path, sizeof(tag_path), "%s/files/%s/%s", mount_point, file_name, tag);

  return (stat(tag_path, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : -1;
}

t_package *prepare_error_response(uint32_t query_id, t_storage_op_code op_error_code) {
  t_package *response = package_create_empty(op_error_code);
  if(!response)
  {
    log_error(g_storage_logger, "## Handshake del Worker %s: no se pudo crear el paquete de respuesta - Socket: %d", client_data->client_id, client_data->client_socket);

    return NULL;
  }
}