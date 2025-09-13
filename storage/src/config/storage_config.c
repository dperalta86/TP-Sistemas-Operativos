#include "storage_config.h"
#include <errno.h>

static bool has_required_properties(t_config *config);
static char *duplicate_config_value(char *value, t_config *config, t_storage_config *storage_config);
static void handle_fatal_error(t_config *config, t_storage_config *storage_config);

t_storage_config *create_storage_config(const char *config_file_path)
{
    t_storage_config *storage_config = malloc(sizeof *storage_config);
    if (!storage_config)
    {
        fprintf(stderr, "No se pudo reservar memoria para storage_config: %s\n", strerror(errno));
        return NULL;
    }

    t_config *config = config_create((char *)config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el config: %s\n", config_file_path);
        free(storage_config);
        return NULL;
    }

    if (!has_required_properties(config))
    {
        fprintf(stderr, "El archivo de configuración no tiene todas las propiedades requeridas: %s\n",
                config_file_path);
        config_destroy(config);
        free(storage_config);
        return NULL;
    }

    storage_config->storage_ip = duplicate_config_value(config_get_string_value(config, "STORAGE_IP"), config, storage_config);
    storage_config->storage_port = duplicate_config_value(config_get_string_value(config, "STORAGE_PORT"), config, storage_config);
    storage_config->fresh_start = duplicate_config_value(config_get_string_value(config, "FRESH_START"), config, storage_config);
    storage_config->module_path = duplicate_config_value(config_get_string_value(config, "MODULE_PATH"), config, storage_config);
    storage_config->operation_delay = duplicate_config_value(config_get_string_value(config, "OPERATION_DELAY"), config, storage_config);
    storage_config->block_access_delay = duplicate_config_value(config_get_string_value(config, "BLOCK_ACCESS_DELAY"), config, storage_config);
    storage_config->log_level = duplicate_config_value(config_get_string_value(config, "LOG_LEVEL"), config, storage_config);

    config_destroy(config);
    return storage_config;
}

void destroy_storage_config(t_storage_config *storage_config)
{
    if (!storage_config)
        return;

    free(storage_config->storage_ip);
    free(storage_config->storage_port);
    free(storage_config->fresh_start);
    free(storage_config->module_path);
    free(storage_config->operation_delay);
    free(storage_config->block_access_delay);
    free(storage_config->log_level);

    free(storage_config);
}

static bool has_required_properties(t_config *config)
{
    char *required_props[] = {
        "STORAGE_IP",
        "STORAGE_PORT",
        "FRESH_START",
        "MODULE_PATH",
        "OPERATION_DELAY",
        "BLOCK_ACCESS_DELAY",
        "LOG_LEVEL"
    };

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

static void handle_fatal_error(t_config *config, t_storage_config *storage_config) {
    fprintf(stderr, "Error fatal, cerrando el programa.\n");
    config_destroy(config);
    destroy_storage_config(storage_config);
    exit(EXIT_FAILURE);
}

static char *duplicate_config_value(char *value, t_config *config, t_storage_config *storage_config)
{
    if (!value)
    {
        fprintf(stderr, "Valor de configuración es NULL\n");
        handle_fatal_error(config, storage_config);
    }
    char *duplicate = strdup(value);
    if (!duplicate)
    {
        fprintf(stderr, "strdup falló\n");
        handle_fatal_error(config, storage_config);
    }
    return duplicate;
}