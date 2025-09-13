#include "query_control_config.h"

t_query_control_config *create_query_control_config(char *config_file_path)
{
    t_config *config = config_create(config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el archivo config %s\n", config_file_path);
        return -1;
    }

    t_query_control_config *query_control_config = malloc(sizeof *query_control_config);
    if (!query_control_config)
    {
        goto error;
    }

    query_control_config->ip = strdup(config_get_string_value(config, "IP_ESCUCHA"));
    query_control_config->port = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
    char *log_level_str = config_get_string_value(config, "LOG_LEVEL");
    if (strcmp(log_level_str, "DEBUG") == 0) {
        query_control_config->log_level = LOG_LEVEL_DEBUG;
    } else if (strcmp(log_level_str, "INFO") == 0) {
        query_control_config->log_level = LOG_LEVEL_INFO;
    } else if (strcmp(log_level_str, "WARNING") == 0) {
        query_control_config->log_level = LOG_LEVEL_WARNING;
    } else if (strcmp(log_level_str, "ERROR") == 0) {
        query_control_config->log_level = LOG_LEVEL_ERROR;
    } else if (strcmp(log_level_str, "CRITICAL") == 0) {
        query_control_config->log_level = LOG_LEVEL_CRITICAL;
    } else {
        fprintf(stderr, "Nivel de log desconocido: %s\n", log_level_str);
        goto error;
    }
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
