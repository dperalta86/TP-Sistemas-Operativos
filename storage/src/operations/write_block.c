#include "write_block.h"
#include <linux/limits.h>

t_package *handle_write_block_request(t_package *package) {
  uint32_t query_id;
  char *name = NULL;
  char *tag = NULL;
  uint32_t block_number;
  size_t data_size = 0;
  void *block_data = NULL;

  if (deserialize_block_write_request(package, &query_id, &name, &tag,
                                      &block_number, &block_data,
                                        &data_size) < 0) {
    return NULL;
  }

  int operation_result =
      execute_block_write(name, tag, query_id, block_number, block_data, data_size);

  free(name);
  free(tag);
  free(block_data);

  t_package *response = package_create_empty(STORAGE_OP_BLOCK_WRITE_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Query ID: %d - Fallo al crear paquete de respuesta.",
              query_id);
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Error al escribir status en respuesta de WRITE BLOCK");
    package_destroy(response);
    return NULL;
  }

  package_reset_read_offset(response);

  return response;
}

int deserialize_block_write_request(t_package *package, uint32_t *query_id,
                                    char **name, char **tag,
                                    uint32_t *block_number,
                                    void **block_data,
                                    size_t *data_size) {
  int retval = 0;

  if (!package_read_uint32(package, query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de WRITE_BLOCK");
    retval = -1;
    goto end;
  }

  *name = package_read_string(package);
  if (*name == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %d - Error al deserializar el nombre del file de "
              "WRITE_BLOCK",
              *query_id);
    retval = -1;
    goto end;
  }

  *tag = package_read_string(package);
  if (*tag == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %d - Error al deserializar el tag del file de "
              "WRITE_BLOCK",
              *query_id);
    retval = -1;
    goto clean_name;
  }

  if (!package_read_uint32(package, block_number)) {
    log_error(g_storage_logger,
              "## Query ID: %d - Error al deserializar el número bloque de "
              "WRITE_BLOCK",
              *query_id);
    retval = -1;
    goto clean_tag;
  }

  *block_data = package_read_data(package, data_size);
  if (*block_data == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %d - Error al deserializar el contenido a escribir "
              "de WRITE_BLOCK",
              *query_id);
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

int create_new_hardlink(uint32_t query_id, const char *name, const char *tag, uint32_t logical_block, char *logical_block_path,
                        ssize_t physical_block_index) {

  size_t bitmap_bits = g_storage_config->bitmap_size_bytes * 8;
  if (physical_block_index < 0 || (size_t)physical_block_index >= bitmap_bits) {
    log_error(g_storage_logger,
              "## Query ID: %d - Índice de bit inválido (%zd). Fuera de rango "
              "[0, %zu).",
              query_id, physical_block_index, bitmap_bits);
    return -1;
  }

  char physical_block_path[PATH_MAX];
  snprintf(physical_block_path, sizeof(physical_block_path),
           "%s/physical_blocks/block%04zd.dat", g_storage_config->mount_point,
           physical_block_index);
  if (link(physical_block_path, logical_block_path) != 0) {
    log_error(g_storage_logger,
              "## Query ID: %d - No se pudo crear el hard link de %s a %s.",
              query_id, physical_block_path, logical_block_path);
    return -1;
  }

  log_info(g_storage_logger, "## Query ID: %" PRIu32 " - %s:%s - Se agregó el hardlink del bloque lógico %" PRIu32 " al bloque físico %zd", query_id, name, tag, logical_block, physical_block_index);

  return 0;
}

int write_to_logical_block(uint32_t query_id, const char *file_name,
                           const char *tag, uint32_t block_number,
                           const void *block_data, size_t data_size) {
  char logical_block_path[PATH_MAX];
  snprintf(logical_block_path, sizeof(logical_block_path),
           "%s/files/%s/%s/logical_blocks/%04d.dat",
           g_storage_config->mount_point, file_name, tag, block_number);

  FILE *block_file = fopen(logical_block_path, "r+b");
  if (block_file == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %d - No se pudo abrir el bloque %s para escritura.",
              query_id, logical_block_path);
    return -1;
  }

  usleep(g_storage_config->block_access_delay * 1000);

  size_t block_size = g_storage_config->block_size;
  void *buffer = calloc(1, block_size);
  if (buffer == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %d - Error al escribir en el bloque %s.", query_id,
              logical_block_path);
    fclose(block_file);
    return -1;
  }

  size_t bytes_to_copy = data_size < block_size ? data_size : block_size;
  memcpy(buffer, block_data, bytes_to_copy);

  size_t bytes_written = fwrite(buffer, 1, block_size, block_file);
  free(buffer);

  if (bytes_written != block_size) {
    log_error(g_storage_logger,
              "## Query ID: %d - Error al escribir en el bloque %s.", query_id,
              logical_block_path);
    fclose(block_file);
    return -1;
  }

  log_info(g_storage_logger,
           "## Query ID: %" PRIu32 " - Bloque lógico escrito %s:%s - Número de bloque: %" PRIu32,
           query_id, file_name, tag, block_number);

  fclose(block_file);
  return 0;
}

int execute_block_write(const char *name, const char *tag, uint32_t query_id,
                        uint32_t block_number, const void *block_data, size_t data_size){
  t_bitarray *bitmap = NULL;
  char *bitmap_buffer = NULL;
  int retval = 0;

  lock_file(name, tag, true);

  if (!file_dir_exists(name, tag)) {
    log_error(g_storage_logger,
              "## Query ID: %d - El directorio %s:%s no existe.", query_id,
              name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  t_file_metadata *metadata =
      read_file_metadata(g_storage_config->mount_point, name, tag);
  if (metadata == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %d - No se pudo leer el metadata de %s:%s.",
              query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  if (strcmp(metadata->state, COMMITTED) == 0) {
    log_error(g_storage_logger,
              "## Query ID: %d - El archivo %s:%s ya está en estado "
              "'COMMITTED' y no puede ser escrito.",
              query_id, name, tag);
    retval = FILE_ALREADY_COMMITTED;
    goto cleanup_metadata;
  }

  if ((int)block_number >= metadata->block_count) {
    log_error(g_storage_logger,
              "## Query ID: %d - El bloque lógico %d no existe en %s:%s. Fuera "
              "de rango [0, %d]",
              query_id, block_number, name, tag, metadata->block_count);
    retval = READ_OUT_OF_BOUNDS;
    goto cleanup_metadata;
  }

  char logical_block_path[PATH_MAX];
  snprintf(logical_block_path, sizeof(logical_block_path),
           "%s/files/%s/%s/logical_blocks/%04d.dat",
           g_storage_config->mount_point, name, tag, block_number);
  int num_hardlinks = ph_block_links(logical_block_path);
  if (num_hardlinks < 0) {
    retval = -1;
    goto cleanup_metadata;
  }

  if (num_hardlinks > 2) {
    if (remove(logical_block_path) != 0) {
      log_error(g_storage_logger,
                "## Query ID: %d - No se pudo eliminar el hardlink %s.",
                query_id, logical_block_path);
      retval = -2;
      goto cleanup_metadata;
    }

    if (bitmap_load(&bitmap, &bitmap_buffer) < 0) {
      log_error(g_storage_logger, "# Query ID: %d - Fallo al cargar el bitmap.",
                query_id);
      retval = -3;
      goto cleanup_metadata;
    }

    ssize_t physical_block_index = get_free_bit_index(bitmap);
    if (physical_block_index < 0) {
      log_error(g_storage_logger,
                "## Query ID: %d - No hay bloques físicos libres disponibles "
                "en el bitmap.",
                query_id);
      retval = NOT_ENOUGH_SPACE;
      goto unlock_bitmap;
    }

    bitarray_set_bit(bitmap, (off_t)physical_block_index);

    if (bitmap_persist(bitmap, bitmap_buffer) < 0) {
      retval = -4;
      goto cleanup_bitmap;
    }

    log_info(g_storage_logger, "Query ID: %" PRIu32 " - Bloque físico reservado - Número de bloque: %zd", query_id, physical_block_index);

    if (create_new_hardlink(query_id, name, tag, block_number, logical_block_path,
                            physical_block_index) < 0) {
      retval = -5;
      goto cleanup_metadata;
    }

    metadata->blocks[block_number] = (uint32_t)physical_block_index;
    if (save_file_metadata(metadata) < 0) {
      log_error(g_storage_logger,
                "## No se pudo guardar el metadata de %s:%s después de "
                "actualizar bloques.",
                name, tag);
      retval = -6;
      goto cleanup_metadata;
    }
  }

  destroy_file_metadata(metadata);

  if (write_to_logical_block(query_id, name, tag, block_number, block_data, data_size) < 0) {
    retval = -7;
  }


  goto cleanup_unlock;

unlock_bitmap:
  pthread_mutex_unlock(&g_storage_bitmap_mutex);
cleanup_bitmap:
  if (bitmap_buffer)
    free(bitmap_buffer);
  if (bitmap)
    bitarray_destroy(bitmap);
cleanup_metadata:
  if (metadata)
    destroy_file_metadata(metadata);
cleanup_unlock:
  unlock_file(name, tag);

  return retval;
}
