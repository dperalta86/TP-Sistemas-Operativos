#ifndef CONFIG_WORKER_H
#define CONFIG_WORKER_H

#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

typedef struct {
    char* master_ip;
    char* master_port;
    char* storage_ip;
    char* storage_port;
    int memory_size;
    int memory_retardation;
    char* replacement_algorithm;
    char* path_scripts;
    char* log_level;
} t_worker_config;

/**
 * Crea la configuración del Worker y la devuelve
 * 
 * @param config_file_path Ruta absoluta del archivo de configuración
 * @warning Cuando se deja de usar, debe ser destruido con destroy_worker_config
 */
t_worker_config* create_worker_config(char* config_file_path);

/**
 * Elimina de la memoria la configuración de Worker y todos sus atributos
 * 
 * @param worker_config La struct de configuracion del Worker
 */
void destroy_worker_config(t_worker_config* worker_config);



#endif
