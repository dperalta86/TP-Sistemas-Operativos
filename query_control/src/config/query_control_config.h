#ifndef QUERY_CONTROL_CONFIG_H
#define QUERY_CONTROL_CONFIG_H

#include <commons/config.h>
#include <commons/log.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *ip;
    char *port;
    t_log_level log_level;
} t_query_control_config;

/**
 * Crea la configuración del Query Control y la devuelve
 *
 * @param config_file_path Ruta absoluta del archivo de configuración
 * @return Devuelve la configuración del Query Control o -1 si falla
 * @warning Cuando se deja de usar, debe ser destruido con destroy_query_control_config_instance
 */
t_query_control_config *create_query_control_config(char *config_file_path);

/**
 * Elimina de la memoria la configuración del Query Control y todos sus atributos
 *
 * @param query_control_config La struct de configuracion del Query Control
 */
void destroy_query_control_config_instance(t_query_control_config *query_control_config);

#endif // QUERY_CONTROL_CONFIG_H
