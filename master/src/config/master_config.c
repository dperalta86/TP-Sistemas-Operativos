#include "master_config.h"


t_master_config *create_master_config(char *config_file_path)
{
    t_config *config = config_create(config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el archivo config %s\n", config_file_path);
        return NULL;
    }

    t_master_config *master_config = malloc(sizeof *master_config);
    if (!master_config)
    {
        goto error;
    }

    // Leo las variables de configuracion
    master_config->ip = strdup(config_get_string_value(config, "IP_ESCUCHA"));
    master_config->port = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
    master_config->scheduler_algorithm = strdup(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
    master_config->aging_time = config_get_int_value(config, "TIEMPO_AGING");

    config_destroy(config);
    
    return master_config;

error:
    config_destroy(config);
    destroy_config(master_config);
    return NULL;
}

void destroy_config(t_master_config *master_config)
{
    if (!master_config)
        return;

    free(master_config->ip);
    free(master_config->port);
    free(master_config->scheduler_algorithm);
    free(master_config);
}