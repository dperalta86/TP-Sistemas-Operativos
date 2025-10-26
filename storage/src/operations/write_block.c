#include "write_block.h"

t_package *write_block(t_package *package) {
    uint32_t query_id;
    char *name = NULL;
    char *tag = NULL;
    uint32_t block_number;
    char *block_content = NULL;
    t_package *retval = NULL;

    if (validate_deserialization(package, &query_id, &name, &tag, &block_number, &block_content) < 0) {
        goto end;
    }

    lock_file(name, tag);

    if(!file_dir_exists(name, tag))     
    {
      log_error(g_storage_logger, "## Query ID: %d - El directorio %s:%s no existe.", query_id, name, tag);
      retval = prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, FILE_TAG_MISSING);
      goto cleanup_unlock;
    }

    t_file_metadata *metadata = read_file_metadata(g_storage_config->mount_point, name, tag);
    if (metadata == NULL)
    {
      log_error(g_storage_logger, "## Query ID: %d - No se pudo leer el metadata de %s:%s.", query_id, name, tag);
      retval = prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, FILE_TAG_MISSING);
      goto cleanup_unlock;
    }
    
    if (strcmp(metadata->state, COMMITTED) == 0)
    {
      log_error(g_storage_logger, "## Query ID: %d - El archivo %s:%s ya está en estado 'COMMITTED' y no puede ser escrito.", query_id, name, tag);
      retval = prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, FILE_ALREADY_COMMITTED);
      goto cleanup_metadata;
    }

    char logical_block_path[PATH_MAX];
    if(!logical_block_exists(name, tag, block_number, logical_block_path))
    {
      log_error(g_storage_logger, "## Query ID: %d - El bloque lógico %d no existe en %s:%s.", query_id, block_number, name, tag);
      retval = prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, OP_OUT_OF_BOUNDS);
      goto cleanup_metadata;
    }

    if (!ph_block_has_many_hl(logical_block_path))
    {
      if (remove(logical_block_path) != 0)
      {
        log_error(g_storage_logger, "## Query ID: %d - No se pudo eliminar el hardlink %s.", query_id, logical_block_path);
        retval = NULL;
        goto cleanup_metadata;
      }
      
      pthread_mutex_lock(&g_storage_bitmap_mutex);

      t_bitarray *bitmap = NULL;
      char *bitmap_buffer = NULL;
      if(bitmap_load(&bitmap, &bitmap_buffer) < 0)
      {
        log_error(g_storage_logger, "# Query ID: %d - Fallo al cargar el bitmap.", query_id);
        retval = NULL;
        goto cleanup_bitmap_mutex;
      }

      ssize_t bit_index = get_free_bit_index(bitmap);
      if (bit_index < 0)
      {
        log_error(g_storage_logger, "## Query ID: %d - No hay bloques físicos libres disponibles en el bitmap.", query_id);
        retval = prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, NOT_ENOUGH_SPACE);
        goto cleanup_bitmap;
      }
      
      bitarray_set_bit(bitmap, (off_t)bit_index);

      if (bitmap_persist(bitmap, bitmap_buffer) < 0)
      {
        retval = NULL;
        goto cleanup_bitmap;
      }

      free(bitmap_buffer);  
      destroy_bitmap();
      pthread_mutex_unlock(&g_storage_bitmap_mutex);

      char physical_block_path[PATH_MAX];
      if(create_new_hardlink(query_id, logical_block_path, bit_index, physical_block_path) < 0)
      {
        retval = NULL;
        goto cleanup_metadata;
      }
      
      metadata->blocks[block_number] = (uint32_t)physical_block;
      if(save_file_metadata(metadata) < 0) {
        log_error(g_storage_logger, "## No se pudo guardar el metadata de %s:%s después de actualizar bloques.", name, tag);
        retval = NULL;
        goto cleanup_metadata;
      }

      destroy_file_metadata(metadata);
    }

    if(write_in_ph_block(query_id, logical_block_path, block_content) < 0)
    {
      retval = NULL;
      goto cleanup_unlock;
    }
    
    unlock_file(name, tag);

  retval = package_create_empty(STORAGE_OP_BLOCK_WRITE_RES);
  if(!retval)
  {
    log_error(g_storage_logger, "## Query ID: %d - Fallo al crear paquete de respuesta.", query_id);
    retval = NULL;
  }

  goto cleanup_var;

cleanup_bitmap:
  free(bitmap_buffer);  
  destroy_bitmap();
cleanup_bitmap_mutex:
  pthread_mutex_unlock(&g_storage_bitmap_mutex);
cleanup_metadata:
  destroy_file_metadata(metadata);
cleanup_unlock:
  unlock(name, tag);
cleanup_var:
  free(name);
  free(tag);
  free(block_content);
end:
  return retval;
}

int validate_deserialization(t_package *package, uint32_t *query_id, char **name, char **tag, uint32_t *block_number, char **block_content) {
  int retval = 0;

  if (!package_read_uint32(package, query_id)) 
  {
    log_error(g_storage_logger, "## Error al deserializar query_id de WRITE_BLOCK");
    retval = -1;
    goto end;
  }

  *name = package_read_string(package);
  if (*name == NULL) 
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el nombre del file de WRITE_BLOCK", *query_id);
    retval = -1;
    goto end;
  }
  
  *tag = package_read_string(package);
  if (*tag == NULL) 
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el tag del file de WRITE_BLOCK", *query_id);
    retval = -1;
    goto clean_name;
  }

  if(!package_read_uint32(package, block_number))
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el número bloque de WRITE_BLOCK", *query_id);
    retval = -1;
    goto clean_tag;
  }

  *block_content = package_read_string(package);
  if(*block_content == NULL)
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el contenido a escribir de WRITE_BLOCK", *query_id);
    retval = -1;
    goto clean_tag;
  }

  return retval;

clean_tag:
  if(*tag) free(*tag);
clean_name:
  if(*name) free(*name);
end:
  return retval;
}

int create_new_hardlink(uint32_t query_id, char *logical_block_path, ssize_t bit_index, char *physical_block_path) {
    char new_physical_block_path[PATH_MAX];
    snprintf(new_physical_block_path, sizeof(new_physical_block_path), "%s/physical_blocks/block%04zd.dat", g_storage_config->mount_point, bit_index);

    if (!physical_block_exists((uint32_t)bit_index, physical_block_path))
    {
      log_error(g_storage_logger, "## Query ID: %d - El archivo %s no existe.", query_id, physical_block_path);
      return -1;
    }

    if (link(new_physical_block_path, logical_block_path) != 0) 
    {
      log_error(g_storage_logger, "## Query ID: %d - No se pudo crear el hard link de %s a %s.", query_id, new_physical_block_path, logical_block_path);
      return -1;
    }

    return 0;
}

int write_in_ph_block(uint32_t query_id, const char *logical_block_path, const char *block_content) {
    FILE *block_file = fopen(logical_block_path, "r+b");
    if (block_file == NULL) {
        log_error(g_storage_logger, "## Query ID: %d - No se pudo abrir el bloque %s para escritura.", query_id, logical_block_path);
        return -1;
    }

    size_t bytes_written = fwrite(block_content, sizeof(char), strlen(block_content), block_file);
    if (bytes_written != strlen(block_content)) {
        log_error(g_storage_logger, "## Query ID: %d - Error al escribir en el bloque físico %s.", query_id, logical_block_path);
        fclose(block_file);
        return -1;
    }

    fclose(block_file);
    return 0;
}
