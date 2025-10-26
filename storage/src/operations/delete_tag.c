#include "delete_tag.h"
#include "errors.h"
#include "file_locks.h"
#include <string.h>

int delete_tag(uint32_t query_id, const char *name, const char *tag,
               const char *mount_point) {
  int retval = 0;

  if (strcmp(name, "initial_file") == 0 && strcmp(tag, "BASE") == 0) {
    log_warning(g_storage_logger,
                "## %u - No se puede eliminar initial_file:BASE, es un archivo "
                "protegido",
                query_id);
    return -1;
  }

  lock_file(name, tag);

  t_file_metadata *metadata = read_file_metadata(mount_point, name, tag);
  if (!metadata) {
    log_warning(g_storage_logger,
                "## %u - No se pudo leer el metadata para %s:%s, "
                "reportando FILE_TAG_MISSING",
                query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto end;
  }

  for (int i = 0; i < metadata->block_count; i++) {
    int physical_block_index = metadata->blocks[i];

    if (delete_logical_block(mount_point, name, tag, i, physical_block_index,
                             query_id) != 0) {
      log_error(g_storage_logger,
                "## %u - Error al eliminar el bloque lógico %04d de %s:%s",
                query_id, i, name, tag);
      retval = -2;
      goto clean_metadata;
    }
  }

  if (delete_file_dir_structure(mount_point, name, tag) != 0) {
    log_error(g_storage_logger, "No se pudo eliminar la carpeta %s/%s", name,
              tag);
    retval = -3;
    goto clean_metadata;
  }

  log_info(g_storage_logger, "## %u - Tag Eliminado %s:%s", query_id, name,
           tag);

clean_metadata:
  destroy_file_metadata(metadata);
end:
  unlock_file(name, tag);

  return retval;
}

t_package *handle_delete_tag_op_package(t_package *package) {
  uint32_t query_id;
  if (!package_read_uint32(package, &query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de DELETE_TAG");
    return NULL;
  }

  char *name = package_read_string(package);
  char *tag = package_read_string(package);

  if (!name || !tag) {
    log_error(g_storage_logger,
              "## Error al deserializar parámetros de DELETE_TAG");
    if (name)
      free(name);
    if (tag)
      free(tag);
    return NULL;
  }

  int operation_result =
      delete_tag(query_id, name, tag, g_storage_config->mount_point);

  free(name);
  free(tag);

  t_package *response = package_create_empty(STORAGE_OP_TAG_DELETE_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Error al crear el paquete de respuesta para DELETE_TAG");
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Error al escribir status en respuesta de DELETE_TAG");
    package_destroy(response);
    return NULL;
  }

  return response;
}
