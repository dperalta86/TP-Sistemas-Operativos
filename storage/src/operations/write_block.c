#include "write_block.h"

t_package *write_block(t_package *package, t_client_data *client_data) {
    char *name;
    char *tag;
    uint32_t block_number;
    char *block_content;

    int validation_result = validate_deserialization(package, client_data->query_id, name, tag, &block_number, block_content);
    if(validation_result == -1) 
    {
      return NULL;
    }

    if(file_dir_exists(g_storage_config->mount_point, name, tag) == -1) 
    {
      log_error(g_storage_logger, "## Query ID: %d - El directorio %s:%s no existe.", client_data->query_id, name, tag);
      free(name); // SE LIBERA?
      free(tag); // SE LIBERA?
      // TODO: ARMAR PAQUETE DE RESPUESTA CON STATUS DE ERROR
    }








  t_package *response = package_create_empty(STORAGE_OP_BLOCK_WRITE_RES);
  return response;
}

int validate_deserialization(t_package *package, char *query_id, char *name, char *tag, uint32_t *block_number, char *block_content) {
  name = package_read_string(package);
  if (name == NULL) 
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el nombre del file de WRITE_BLOCK", client_data->query_id);
    return -1;
  }
  
  tag = package_read_string(package);
  if (tag == NULL) 
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el tag del file de WRITE_BLOCK", client_data->query_id);
    free(name);
    return -1;
  }

  if(!package_read_uint32(package, block_number))
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el número bloque de WRITE_BLOCK", client_data->query_id);
    free(name);
    free(tag);
    return -1;
  }

  content = package_read_string(package);
  if(content == NULL)
  {
    log_error(g_storage_logger, "## Query ID: %d - Error al deserializar el contenido a escribir de WRITE_BLOCK", client_data->query_id);
    free(name);
    free(tag);
    return -1;
  }

  return 0;
}