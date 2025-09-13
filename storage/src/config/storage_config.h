#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

typedef struct {
    char* storage_ip;
    char* storage_port;
    char* fresh_start;
    char* module_path;
    char* operation_delay;
    char* block_access_delay;
    char* log_level;
} t_storage_config;

/**
 * Crea la configuración del Storage y la devuelve
 * 
 * @param config_file_path Ruta absoluta del archivo de configuración
 * @warning Cuando se deja de usar, debe ser destruido con destroy_storage_config
 */
t_storage_config* create_storage_config(const char* config_file_path);

/**
 * Elimina de la memoria la configuración de Storage y todos sus atributos
 * 
 * @param storage_config La struct de configuración del storage
 */
void destroy_storage_config(t_storage_config* storage_config);

#endif