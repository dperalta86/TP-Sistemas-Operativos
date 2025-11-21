#include "commit_tag.h"

t_package *handle_tag_commit_request(t_package *package) {
  uint32_t query_id;
  char *name = NULL;
  char *tag = NULL;

  if (deserialize_tag_commit_request(package, &query_id, &name, &tag) < 0) {
    return NULL;
  }

  int operation_result = execute_tag_commit(query_id, name, tag);

  free(name);
  free(tag);

  t_package *response = package_create_empty(STORAGE_OP_TAG_COMMIT_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - Fallo al crear paquete de respuesta.",
              query_id);
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error al escribir status en respuesta de COMMIT TAG",
              query_id);
    package_destroy(response);
    return NULL;
  }

  package_reset_read_offset(response);

  return response;
}

int deserialize_tag_commit_request(t_package *package, uint32_t *query_id,
                                   char **name, char **tag) {
  int retval = 0;

  if (!package_read_uint32(package, query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de COMMIT_TAG");
    retval = -1;
    goto end;
  }

  *name = package_read_string(package);
  if (*name == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - Error al deserializar el nombre del "
              "file para la operación COMMIT_TAG",
              *query_id);
    retval = -2;
    goto end;
  }

  *tag = package_read_string(package);
  if (*tag == NULL) {
    log_error(
        g_storage_logger,
        "## Query ID: %" PRIu32
        " - Error al deserializar el tag del file para la operación COMMIT_TAG",
        *query_id);
    retval = -3;
    goto clean_name_tag;
  }

  return retval;

clean_name_tag:
  if (*tag) {
    free(*tag);
    *tag = NULL;
  }
  if (*name) {
    free(*name);
    *name = NULL;
  }
end:
  return retval;
}

int execute_tag_commit(uint32_t query_id, const char *name, const char *tag) {
  int retval = 0;

  lock_file(name, tag);

  if (!file_dir_exists(name, tag)) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - El directorio %s:%s no existe.",
              query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  t_file_metadata *metadata =
      read_file_metadata(g_storage_config->mount_point, name, tag);
  if (metadata == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo leer el metadata de %s:%s.",
              query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  if (strcmp(metadata->state, COMMITTED) == 0) {
    log_info(g_storage_logger,
             "## Query ID: %" PRIu32 " - El archivo %s:%s ya está en estado "
             "'COMMITTED'.",
             query_id, name, tag);
    goto cleanup_metadata;
  }

  deduplicate_blocks(query_id, name, tag, metadata);

  free(metadata->state);
  metadata->state = strdup(COMMITTED);
  if (!metadata->state) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error de asignación de memoria al duplicar estado COMMITTED.",
              query_id);
    retval = -1;
    goto cleanup_metadata;
  }

  if (save_file_metadata(metadata) < 0) {
    log_error(
        g_storage_logger,
        "## Query ID: %" PRIu32
        " - No se pudo actualizar el estado de metadata de %s:%s a COMMITTED.",
        query_id, name, tag);
    retval = -2;
    goto cleanup_metadata;
  }

  log_info(g_storage_logger,
           "## Query ID: %" PRIu32 " - Commit de file:tag %s:%s.", query_id,
           name, tag);

cleanup_metadata:
  if (metadata)
    destroy_file_metadata(metadata);
cleanup_unlock:
  unlock_file(name, tag);

  return retval;
}

int deduplicate_blocks(uint32_t query_id, const char *name, const char *tag,
                       t_file_metadata *metadata) {
  int retval = 0;

  // Verifica que el file:tag tiene bloques lógicos.
  if (metadata->block_count == 0) {
    log_warning(g_storage_logger,
                "## Query ID: %" PRIu32
                " - File %s:%s no tiene bloques lógicos para deduplicar.",
                query_id, name, tag);
    goto end;
  }

  // Carga el config para blocks_hash_index
  pthread_mutex_lock(g_blocks_hash_index_mutex);
  char hash_index_config_path[PATH_MAX];
  snprintf(hash_index_config_path, sizeof(hash_index_config_path),
           "%s/blocks_hash_index.config", storage_config->mount_point);
  t_config *hash_index_config = config_load(hash_index_config);

  if (hash_index_config == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo leer el archivo blocks_hash_index.config",
              query_id);
    retval = -1;
    goto unlock_hash_index;
  }

  // Itera sobre los bloques lógicos del file:tag para deduplicar
  for (int i = 0; i < metadata->block_count; i++) {
    int logical_block = metadata->blocks[i];

    // Lee el contenido del bloque físico asociado al bloque lógico
    char logical_block_path[PATH_MAX];
    snprintf(logical_block_path, sizeof(logical_block_path),
             "%s/files/%s/%s/logical_blocks/%04d.dat",
             g_storage_config->mount_point, name, tag, logical_block);

    char *read_buffer = (char *)malloc(g_storage_config->block_size);
    if (read_buffer == NULL) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Error de asignación de memoria para leer el bloque: %s",
                query_id, logical_block_path);
      retval = -2;
      goto unlock_hash_index;
    }

    if (read_block_content(query_id, logical_block_path,
                           g_storage_config->block_size, read_buffer) < 0) {
      retval = -3;
      goto cleanup_buffer;
    }

    // Hashea el contenido del bloque leído
    char *hash = crypto_md5(read_buffer, (size_t)g_storage_config->block_size);
    if (hash == NULL) {
      log_error(g_storage_config,
                "## Query ID: %" PRIu32
                " - No se generó el hash para el bloque %s",
                query_id, logical_block_path);
      retval = -3;
      goto cleanup_buffer;
    }

    free(read_buffer);

    // Si blocks_hash_index contiene el hash, elimina el archivo del bloque
    // lógico actual y lo vuelve a crear como hardlink del bloque físico
    // registrado en blocks_hash_index
    if (config_has_property(hash_index_config, hash)) {
      char *physical_block =
          strdup(config_get_string_value(hash_index_config, hash));

      if (remove(logical_block_path) != 0) {
        log_error(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Ocurrió un error al eliminar el bloque lógico %s",
                  query_id, logical_block_path);
        retval = -4;
        goto unlock_hash_index;
      }

      char physical_block_path[PATH_MAX];
      snprintf(physical_block_path, sizeof(physical_block_path),
               "%s/physical_blocks/%s", g_storage_config->mount_point,
               physical_block);
      if (link(physical_block_path, logical_block_path) != 0) {
        log_error(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - No se pudo crear el hard link de %s a %s.",
                  query_id, physical_block_path, logical_block_path);
        retval = -5;
        goto unlock_hash_index;
      }

      log_info(g_storage_logger,
               "## Query ID: %" PRIu32 " - %s:%s - Se agregó el hardlink del "
                                       "bloque lógico %d al bloque físico %s",
               query_id, name, tag, logical_block, physical_block);
    }
  }

cleanup_buffer:
  if (read_buffer)
    free(read_buffer);
unlock_hash_index:
  if (hash_index_config)
    destroy_config(hash_index_config);
  pthread_mutex_unlock(g_blocks_hash_index_mutex);
end:
  return retval;
}

int read_block_content(uint32_t query_id, const char *logical_block_path,
                       uint32_t block_size, char *read_buffer) {
  int retval = 0;

  FILE *file = fopen(logical_block_path, "rb");
  if (file == NULL) {
    log_error(g_storage_logger,
              "Query ID: %" PRIu32
              " - Error al abrir el archivo de bloque lógico: %s",
              query_id, logical_block_path);
    retval = -1;
    goto end;
  }

  size_t read_bytes = fread(read_buffer, 1, block_size, file);

  if (read_bytes < block_size) {
    if (feof(file)) {
      // Lectura parcial
      memset(read_buffer + read_bytes, 0, block_size - read_bytes);
      log_warning(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Bloque lógico %s leído, pero su tamaño (%zu bytes) era "
                  "menor que el tamaño de bloque estándar (%u). Rellenado con "
                  "ceros para hashing.",
                  query_id, logical_block_path, read_bytes, block_size);
    } else if (ferror(file)) {
      // Error de lectura
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Error de lectura en el bloque lógico: %s",
                query_id, logical_block_path);
      retval = -2;
    }
  }

close_file:
  if (file)
    fclose(file);
end:
  return retval;
}
