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

  lock_file(name, tag, false);

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

  if (deduplicate_blocks(query_id, name, tag, metadata) < 0) {
    log_error(g_storage_logger,
              "Query ID: %" PRIu32 " - Error en la deduplicación de bloques.",
              query_id);
    retval = -1;
    goto cleanup_metadata;
  }

  free(metadata->state);
  metadata->state = strdup(COMMITTED);
  if (!metadata->state) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error de asignación de memoria al duplicar estado COMMITTED.",
              query_id);
    retval = -2;
    goto cleanup_metadata;
  }

  if (save_file_metadata(metadata) < 0) {
    log_error(
        g_storage_logger,
        "## Query ID: %" PRIu32
        " - No se pudo actualizar el estado de metadata de %s:%s a COMMITTED.",
        query_id, name, tag);
    retval = -3;
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
  pthread_mutex_lock(&g_blocks_hash_index_mutex);
  char hash_index_config_path[PATH_MAX];
  snprintf(hash_index_config_path, sizeof(hash_index_config_path),
           "%s/blocks_hash_index.config", g_storage_config->mount_point);
  t_config *hash_index_config = config_create(hash_index_config_path);

  if (hash_index_config == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo leer el archivo blocks_hash_index.config",
              query_id);
    retval = -1;
    goto unlock_hash_index;
  }

  // Itera sobre los bloques lógicos del file:tag para deduplicar
  for (int logical_block = 0; logical_block < metadata->block_count;
       logical_block++) {
    int physical_block = metadata->blocks[logical_block];

    char *read_buffer = NULL;
    char *hash = NULL;
    char *physical_block_from_hash = NULL;

    // Lee el contenido del bloque físico asociado al bloque lógico
    char logical_block_path[PATH_MAX];
    snprintf(logical_block_path, sizeof(logical_block_path),
             "%s/files/%s/%s/logical_blocks/%04d.dat",
             g_storage_config->mount_point, name, tag, logical_block);

    read_buffer = (char *)malloc(g_storage_config->block_size);
    if (read_buffer == NULL) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Error de asignación de memoria para leer el bloque: %s",
                query_id, logical_block_path);
      retval = -2;
      goto cleanup_all;
    }

    if (read_block_content(query_id, logical_block_path,
                           g_storage_config->block_size, read_buffer) < 0) {
      retval = -3;
      goto cleanup_loop;
    }

    // Hashea el contenido del bloque leído
    hash = crypto_md5(read_buffer, (size_t)g_storage_config->block_size);
    if (hash == NULL) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - No se generó el hash para el bloque %s",
                query_id, logical_block_path);
      retval = -3;
      goto cleanup_loop;
    }

    free(read_buffer);
    read_buffer = NULL;

    // Obtiene el bloque físico vinculado al actual bloque lógico de la
    // iteración
    char physical_block_from_logical_path[PATH_MAX];
    snprintf(physical_block_from_logical_path,
             sizeof(physical_block_from_logical_path), "block%04d",
             physical_block);

    // Verifica que blocks_hash_index contenga el hash
    if (!config_has_property(hash_index_config, hash)) {
      // Registro del nuevo Hash
      config_set_value(hash_index_config, hash,
                       physical_block_from_logical_path);

      log_info(g_storage_logger,
               "## Query ID: %" PRIu32 " - Hash registrado para el bloque "
               "físico %s en blocks hash index config.",
               query_id, physical_block_from_logical_path);

      goto cleanup_loop;
    }

    // Obtiene el bloque físico vinculado al hash en hash index config
    physical_block_from_hash =
        strdup(config_get_string_value(hash_index_config, hash));
    if (physical_block_from_hash == NULL) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Error al duplicar cadena para bloque físico desde hash "
                "index config.",
                query_id);
      retval = -4;
      goto cleanup_loop;
    }

    // Si ambos bloques físicos son diferentes, debe deduplicar
    if (strcmp(physical_block_from_hash, physical_block_from_logical_path) !=
        0) {
      // El hash existe, pero apunta a otro bloque físico. Reasignamos el
      // bloque lógico.
      log_debug(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Bloque %s es DUPLICADO. Reasignando a bloque físico %s.",
                query_id, logical_block_path, physical_block_from_hash);

      // Redirige el hardlink del bloque lógico al bloque físico existente
      if (update_logical_block_link(query_id, logical_block_path,
                                    physical_block_from_hash) < 0) {
        log_error(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Fallo en la reasignación del link para %s.",
                  query_id, logical_block_path);
        retval = -5;
        goto cleanup_loop;
      }

      int32_t new_physical_id =
          get_physical_block_number(physical_block_from_hash);
      if (new_physical_id < 0) {
        log_error(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - No se pudo obtener el ID del nuevo bloque físico %s.",
                  query_id, physical_block_from_hash);
        retval = -6;
        goto cleanup_loop;
      }

      // Actualiza la metadata con el ID del bloque físico compartido y la
      // cantidad de bloques
      metadata->blocks[logical_block] = new_physical_id;
      log_debug(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Actualizando metadata de %s:%s - Bloque lógico %d a "
                "bloque físico %s.",
                query_id, name, tag, logical_block, physical_block_from_hash);

      // Maneja la liberación del bloque físico anterior (si ya no tiene
      // referencias)
      if (free_ph_block_if_unused(query_id, physical_block_from_logical_path) <
          0) {
        log_error(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - El bloque físico %s no se pudo actualizar como libre "
                  "en el bitmap.",
                  query_id, physical_block_from_logical_path);
        retval = -7;
        goto cleanup_loop;
      }

      log_info(g_storage_logger,
               "## Query ID: %" PRIu32
               " - %s:%s - Bloque Lógico %d se reasigna de %s a %s",
               query_id, name, tag, logical_block,
               physical_block_from_logical_path, physical_block_from_hash);

    } else {
      // El hash ya está en el índice y apunta al bloque físico correcto
      log_info(g_storage_logger,
               "## Query ID: %" PRIu32
               " - Bloque %s ya está correctamente asociado.",
               query_id, logical_block_path);
    }

  cleanup_loop:
    if (physical_block_from_hash)
      free(physical_block_from_hash);
    if (hash)
      free(hash);
    if (read_buffer)
      free(read_buffer);

    if (retval < 0)
      goto cleanup_all;
  }

  config_save(hash_index_config);

cleanup_all:
  if (hash_index_config)
    config_destroy(hash_index_config);
unlock_hash_index:
  pthread_mutex_unlock(&g_blocks_hash_index_mutex);
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

  // Retardo por lectura de bloque
  usleep(g_storage_config->block_access_delay * 1000);

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

  if (file)
    fclose(file);

end:
  return retval;
}

int get_current_physical_block(uint32_t query_id,
                               const char *logical_block_path,
                               char *physical_block_name) {
  int retval = -3;
  struct stat logical_stat;
  struct stat physical_stat;
  DIR *blocks_dir;
  struct dirent *entry;
  char physical_block_path[PATH_MAX];

  // Obtener el i-node del bloque lógico (hard link)
  if (stat(logical_block_path, &logical_stat) < 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Fallo al obtener stat del bloque lógico: %s",
              query_id, logical_block_path);
    return -1;
  }

  // Abrir el directorio que contiene todos los bloques físicos
  char physical_blocks_path[PATH_MAX];
  snprintf(physical_blocks_path, sizeof(physical_blocks_path),
           "%s/physical_blocks/", g_storage_config->mount_point);
  blocks_dir = opendir(physical_blocks_path);
  if (blocks_dir == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo abrir el directorio de bloques: %s",
              query_id, physical_blocks_path);
    return -2;
  }

  // Iterar sobre todos los archivos en el directorio de bloques físicos
  while ((entry = readdir(blocks_dir)) != NULL) {
    // Ignorar "." y ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

// Construir la ruta completa al bloque físico actual
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(physical_block_path, sizeof(physical_block_path), "%s/%s",
             physical_blocks_path, entry->d_name);
#pragma GCC diagnostic pop

    // Obtener el stat del archivo físico
    if (stat(physical_block_path, &physical_stat) < 0) {
      log_warning(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Fallo al obtener stat de %s. Ignorando.",
                  query_id, physical_block_path);
      continue;
    }

    // Comparar i-nodes
    if (logical_stat.st_ino == physical_stat.st_ino) {
      // Devolver el nombre del archivo físico
      strncpy(physical_block_name, entry->d_name, PATH_MAX);
      physical_block_name[PATH_MAX - 1] = '\0';

      log_debug(g_storage_logger,
                "Bloque lógico %s apunta al bloque físico: %s",
                logical_block_path, physical_block_name);
      retval = 0;

      break;
    }
  }

  closedir(blocks_dir);

  return retval;
}

int32_t get_physical_block_number(const char *physical_block_id) {
  int32_t block_number;

  int result = sscanf(physical_block_id, "block%" SCNd32, &block_number);

  if (result == 1) {
    return block_number;
  } else {
    log_error(g_storage_logger,
              "Fallo al parsear el número de bloque de la cadena: %s",
              physical_block_id);
    return -1;
  }
}

int update_logical_block_link(uint32_t query_id, const char *logical_block_path,
                              char *physical_block_name) {
  if (remove(logical_block_path) != 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Ocurrió un error al eliminar el bloque lógico %s",
              query_id, logical_block_path);
    return -1;
  }

  char physical_block_path[PATH_MAX];
  snprintf(physical_block_path, sizeof(physical_block_path),
           "%s/physical_blocks/%s.dat", g_storage_config->mount_point,
           physical_block_name);
  if (link(physical_block_path, logical_block_path) != 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo crear el hard link de %s a %s.",
              query_id, physical_block_path, logical_block_path);
    return -2;
  }

  log_debug(g_storage_logger,
            "## Query ID: %" PRIu32 " - Se agregó el hardlink del "
            "bloque lógico %s al bloque físico %s",
            query_id, logical_block_path, physical_block_path);

  return 0;
}

int free_ph_block_if_unused(uint32_t query_id, char *physical_block_name) {
  // Obtenemos el número de referencias (links) del bloque físico
  char physical_block_path[PATH_MAX];
  snprintf(physical_block_path, sizeof(physical_block_path),
           "%s/physical_blocks/%s.dat", g_storage_config->mount_point,
           physical_block_name);

  int numb_links = ph_block_links(physical_block_path);
  if (numb_links < 0) {
    log_error(
        g_storage_logger,
        "Query ID: %" PRIu32
        " - Error al obtener el número actual de links del bloque físico %s",
        query_id, physical_block_name);

    return -1;
  }

  if (numb_links > 1) {
    log_info(g_storage_logger,
             "Query ID: %" PRIu32
             " - El bloque físico %s continúa siendo referenciado por otros "
             "bloques lógicos. Se mantiene como ocupado en el bitmap.",
             query_id, physical_block_name);

    return 0;
  }

  log_info(g_storage_logger,
           "Query ID: %" PRIu32
           " - El bloque físico %s no es referenciado por ningún bloque "
           "lógico. Se procede a marcarlo como libre en el bitmap.",
           query_id, physical_block_name);

  int32_t physical_block_id = get_physical_block_number(physical_block_name);
  if (physical_block_id < 0) {
    log_error(g_storage_logger,
              "Query ID: %" PRIu32
              " - No se pudo obtener el id de bloque de la cadena %s",
              query_id, physical_block_name);

    return -2;
  }

  t_bitarray *bitmap = NULL;
  char *bitmap_buffer = NULL;
  if (bitmap_load(&bitmap, &bitmap_buffer) < 0) {
    log_error(g_storage_logger,
              "# Query ID: %" PRIu32 " - Fallo al cargar el bitmap.", query_id);
    return -3;
  }

  bitarray_clean_bit(bitmap, (off_t)physical_block_id);

  if (bitmap_persist(bitmap, bitmap_buffer) < 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error al persistir bits libres en el bitmap.",
              query_id);
    return -4;
  }

  return 0;
}