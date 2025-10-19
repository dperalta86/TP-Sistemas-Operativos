#include "write_block.h"

t_package *write_block(t_package *package, t_client_data *client_data) {
    uint32_t query_id;
    char *name = NULL;
    char *tag = NULL;
    uint32_t block_number;
    char *block_content = NULL;

    int validation_result = validate_deserialization(package, &query_id, &name, &tag, &block_number, &block_content);
    if(validation_result < 0) 
    {
      return NULL;
    }

    if(!file_dir_exists(g_storage_config->mount_point, name, tag)) 
    {
      log_error(g_storage_logger, "## Query ID: %d - El directorio %s:%s no existe.", query_id, name, tag);
      free(name);
      free(tag);
      free(block_content);

      return prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, FILE_TAG_ALREADY_EXISTS);
    }

    if(verify_metadata_status(g_storage_config->mount_point, name, tag, COMMITTED))
    {
      log_error(g_storage_logger, "## Query ID: %d - El archivo %s:%s ya está en estado 'COMMITTED' y no puede ser escrito.", query_id, name, tag);
      free(name);
      free(tag);
      free(block_content);

      return prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, FILE_ALREADY_COMMITTED);
    }

    if(!logical_block_exists(g_storage_config->mount_point, name, tag, block_number))
    {
      log_error(g_storage_logger, "## Query ID: %d - El bloque lógico %d no existe en %s:%s.", query_id, block_number, name, tag);
      free(name);
      free(tag);
      free(block_content);

      return prepare_error_response(query_id, STORAGE_OP_BLOCK_WRITE_RES, OP_OUT_OF_BOUNDS);
    }










  t_package *response = package_create_empty(STORAGE_OP_BLOCK_WRITE_RES);
  return response;
}

int validate_deserialization(t_package *package, uint32_t *query_id, char **name, char **tag, uint32_t *block_number, char **block_content) {
  if (!package_read_uint32(package, query_id)) 
  {
    log_error(g_storage_logger, "## Error al deserializar query_id de WRITE_BLOCK");
    return -1;
  }

  *name = package_read_string(package);
  if (*name == NULL) 
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el nombre del file de WRITE_BLOCK", *query_id);
    return -1;
  }
  
  *tag = package_read_string(package);
  if (*tag == NULL) 
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el tag del file de WRITE_BLOCK", *query_id);
    free(*name);
    return -1;
  }

  if(!package_read_uint32(package, block_number))
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el número bloque de WRITE_BLOCK", *query_id);
    free(*name);
    free(*tag);
    return -1;
  }

  *block_content = package_read_string(package);
  if(*block_content == NULL)
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el contenido a escribir de WRITE_BLOCK", *query_id);
    free(*name);
    free(*tag);
    return -1;
  }

  return 0;
}