#include "query_control_config.h"

t_query_control_config *create_query_control_config(char *config_file_path)
{
    t_config *config = config_create(config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el archivo config %s\n", config_file_path);
        return NULL;
    }

    t_query_control_config *query_control_config = malloc(sizeof *query_control_config);
    if (!query_control_config)
    {
        config_destroy(config); 
        fprintf(stderr, "Error al asignar memoria para configuración\n");
        return NULL; 
    }

    query_control_config->ip = strdup(config_get_string_value(config, "IP_MASTER"));
    query_control_config->port = strdup(config_get_string_value(config, "PUERTO_MASTER"));
    
    if (!query_control_config->ip || !query_control_config->port) {
        fprintf(stderr, "Error al duplicar strings de configuración\n");
        destroy_query_control_config_instance(query_control_config);
        config_destroy(config);
        return NULL;
    }
    
    query_control_config->log_level = log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));
    
    config_destroy(config); 

    return query_control_config;
}

void destroy_query_control_config_instance(t_query_control_config *query_control_config)
{
    if (!query_control_config)
        return;

    free(query_control_config->ip);
    free(query_control_config->port);
    free(query_control_config);
}