#include "worker_config.h"

#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))

typedef struct
{
    
    char *key;
    char **field;
} t_str_valid_field;

typedef struct
{
    char *key;
    int *field;
} t_int_valid_field;

t_worker_config *create_worker_config(char *config_file_path)
{
    t_config *config = config_create(config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el archivo config %s\n", config_file_path);
        return NULL;
    }

    t_worker_config *worker_config = malloc(sizeof *worker_config);
    if (!worker_config)
    {
        fprintf(stderr, "No se pudo reservar memoria para worker_config\n");
        config_destroy(config);
        return NULL;
    }

    t_str_valid_field str_fields[] = {
        {"IP_MASTER", &worker_config->master_ip},
        {"PUERTO_MASTER", &worker_config->master_port},
        {"IP_STORAGE", &worker_config->storage_ip},
        {"PUERTO_STORAGE", &worker_config->storage_port},
        {"ALGORITMO_REEMPLAZO", &worker_config->replacement_algorithm},
        {"PATH_SCRIPTS", &worker_config->path_scripts},
        {"LOG_LEVEL", &worker_config->log_level},
    };

    for (size_t i = 0; i < ARRAY_LENGTH(str_fields); i++)
    {
        bool has_property = config_has_property(config, str_fields[i].key);
        if (!has_property)
        {
            fprintf(stderr, "Falta una clave: %s\n", str_fields[i].key);
            goto error;
        }

        const char *original_value = config_get_string_value(config, str_fields[i].key);
        if (!original_value || *original_value == '\0')
        {
            fprintf(stderr, "Clave sin valor: %s\n", str_fields[i].key);
            goto error;
        }

        *str_fields[i].field = strdup(original_value);
        if (!*str_fields[i].field)
        {
            fprintf(stderr, "strdup fallo en: %s\n", str_fields[i].key);
            goto error;
        }
    }

    t_int_valid_field int_fields[] = {
        {"TAM_MEMORIA", &worker_config->memory_size},
        {"RETARDO_MEMORIA", &worker_config->memory_retardation},
    };

    for (size_t i = 0; i < ARRAY_LENGTH(int_fields); i++)
    {
        bool has_property = config_has_property(config, int_fields[i].key);

        if (!has_property)
        {
            fprintf(stderr, "Falta una clave: %s\n", int_fields[i].key);
            goto error;
        }

        int original_value = config_get_int_value(config, int_fields[i].key);
        *int_fields[i].field = original_value;
    }

    config_destroy(config);
    return worker_config;

error:
    config_destroy(config);
    destroy_worker_config(worker_config);
    return NULL;
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
