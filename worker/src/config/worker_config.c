#include "worker_config.h"

static bool has_required_properties(t_config *config);
static char *duplicate_config_value(char *value, t_config *config, t_worker_config *wk);

t_worker_config *create_worker_config(char *config_file_path)
{
    t_worker_config *worker_config = malloc(sizeof *worker_config);
    if (!worker_config)
    {
        fprintf(stderr, "No se pudo reservar memoria para worker_config\n");
        return NULL;
    }

    t_config *config = config_create((char *)config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el config: %s\n", config_file_path);
        free(worker_config);
        return NULL;
    }

    if (!has_required_properties(config))
    {
        fprintf(stderr, "El archivo de configuración no tiene todas las propiedades requeridas: %s\n",
                config_file_path);
        config_destroy(config);
        free(worker_config);
        return NULL;
    }

    worker_config->master_ip = duplicate_config_value(config_get_string_value(config, "IP_MASTER"), config, worker_config);
    worker_config->master_port = duplicate_config_value(config_get_string_value(config, "PUERTO_MASTER"), config, worker_config);
    worker_config->storage_ip = duplicate_config_value(config_get_string_value(config, "IP_STORAGE"), config, worker_config);
    worker_config->storage_port = duplicate_config_value(config_get_string_value(config, "PUERTO_STORAGE"), config, worker_config);
    worker_config->replacement_algorithm = duplicate_config_value(config_get_string_value(config, "ALGORITMO_REEMPLAZO"), config, worker_config);
    worker_config->path_scripts = duplicate_config_value(config_get_string_value(config, "PATH_SCRIPTS"), config, worker_config);
    worker_config->log_level = duplicate_config_value(config_get_string_value(config, "LOG_LEVEL"), config, worker_config);

    worker_config->memory_size = config_get_int_value(config, "TAM_MEMORIA");
    worker_config->memory_retardation = config_get_int_value(config, "RETARDO_MEMORIA");

    config_destroy(config);
    return worker_config;
}

void destroy_worker_config(t_worker_config *worker_config)
{
    if (!worker_config)
        return;

    free(worker_config->master_ip);
    free(worker_config->master_port);
    free(worker_config->storage_ip);
    free(worker_config->storage_port);
    free(worker_config->replacement_algorithm);
    free(worker_config->path_scripts);
    free(worker_config->log_level);

    free(worker_config);
}

static bool has_required_properties(t_config *config)
{
    char *required_props[] = {
        "IP_MASTER",
        "PUERTO_MASTER",
        "IP_STORAGE",
        "PUERTO_STORAGE",
        "TAM_MEMORIA",
        "RETARDO_MEMORIA",
        "ALGORITMO_REEMPLAZO",
        "PATH_SCRIPTS",
        "LOG_LEVEL"};

    size_t n = sizeof(required_props) / sizeof(required_props[0]);
    for (size_t i = 0; i < n; ++i)
    {
        if (!config_has_property(config, required_props[i]))
        {
            fprintf(stderr, "Falta propiedad requerida: %s\n", required_props[i]);
            return false;
        }
    }
    return true;
}

static char *duplicate_config_value(char *value, t_config *config, t_worker_config *worker_config)
{
    if (!value)
    {
        fprintf(stderr, "Valor de configuración es NULL\n");
        config_destroy(config);
        destroy_worker_config(worker_config);
        exit(EXIT_FAILURE);
    }
    char *duplicate = strdup(value);
    if (!duplicate)
    {
        fprintf(stderr, "strdup fallo\n");
        config_destroy(config);
        destroy_worker_config(worker_config);
        exit(EXIT_FAILURE);
    }
    return duplicate;
}
