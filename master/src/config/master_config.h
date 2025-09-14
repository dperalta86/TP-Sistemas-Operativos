#ifndef MASTER_CONFIG_H
#define MASTER_CONFIG_H

#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

typedef struct
{
    char *ip;
    char *port;
    char *scheduler_algorithm;
    int aging_time;
} t_master_config;



/**
 * Crea la configuración del Worker y la devuelve
 *
 * @param config_file_path Ruta absoluta del archivo de configuración
 * @return Devuelve la configuración del worker o NULL si falla
 * @warning Cuando se deja de usar, debe ser destruido con destroy_worker_config
 */
t_master_config *create_master_config(char *config_file_path);

/**
 * Elimina de la memoria la configuración de Worker y todos sus atributos
 *
 * @param masterer_config La struct de configuracion del Worker
 */
void destroy_config(t_master_config *master_config);

#endif