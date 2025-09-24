#include "storage_config.h"
#include <errno.h>

static bool has_required_properties(t_config *config);

t_storage_config *create_storage_config(const char *config_file_path)
{
    t_config *config = config_create((char *)config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el config: %s\n", config_file_path);
        return NULL;
    }

    if (!has_required_properties(config))
    {
        fprintf(stderr, "El archivo de configuración no tiene todas las propiedades requeridas: %s\n", config_file_path);
        goto clean_config;
    }

    t_storage_config *storage_config = malloc(sizeof *storage_config);
    if (!storage_config)
    {
        fprintf(stderr, "No se pudo reservar memoria para storage_config: %s\n", strerror(errno));
        goto clean_config;
    }

    storage_config->operation_delay = config_get_int_value(config, "OPERATION_DELAY");
    storage_config->block_access_delay = config_get_int_value(config, "BLOCK_ACCESS_DELAY");

    char *storage_ip_str = strdup(config_get_string_value(config, "STORAGE_IP"));
    if (!storage_ip_str)
        goto cleanup;
    storage_config->storage_ip = storage_ip_str;

    char *storage_port_str = strdup(config_get_string_value(config, "STORAGE_PORT"));
    if (!storage_port_str)
        goto cleanup;
    storage_config->storage_port = storage_port_str;

    char *mount_point_str = strdup(config_get_string_value(config, "MOUNT_POINT"));
    if (!mount_point_str)
        goto cleanup;
    storage_config->mount_point = mount_point_str;

    char *log_level_str = strdup(config_get_string_value(config, "LOG_LEVEL"));
    if (!log_level_str)
        goto cleanup;
    storage_config->log_level = log_level_from_string(log_level_str);

    char *fresh_start_str = strdup(config_get_string_value(config, "FRESH_START"));
    if (!fresh_start_str)
        goto cleanup;
    storage_config->fresh_start = strcmp(fresh_start_str, "TRUE") == 0 || strcmp(fresh_start_str, "true") == 0 ? true : false;

    free(fresh_start_str);
    free(log_level_str);
    config_destroy(config);
    return storage_config;

cleanup:
    destroy_storage_config(storage_config);

clean_config:
    config_destroy(config);

    return NULL;
}

void destroy_storage_config(t_storage_config *storage_config)
{
    if (!storage_config)
        return;

    free(storage_config->storage_ip);
    free(storage_config->storage_port);
    free(storage_config->mount_point);

    free(storage_config);
}

static bool has_required_properties(t_config *config)
{
    char *required_props[] = {
        "STORAGE_IP",
        "STORAGE_PORT",
        "FRESH_START",
        "MOUNT_POINT",
        "OPERATION_DELAY",
        "BLOCK_ACCESS_DELAY",
        "LOG_LEVEL"
    };

    size_t keys_amount = config_keys_amount(config);
    for (size_t i = 0; i < keys_amount; ++i)
    {
        if (!config_has_property(config, required_props[i]))
        {
            fprintf(stderr, "Falta propiedad requerida: %s\n", required_props[i]);
            return false;
        }
    }
    return true;
}