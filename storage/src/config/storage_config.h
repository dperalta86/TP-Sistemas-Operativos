#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include "globals/globals.h"
#include <commons/config.h>
#include <commons/log.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Crea la configuración del Storage y la devuelve
 *
 * @param config_file_path Ruta absoluta del archivo de configuración
 * @warning Cuando se deja de usar, debe ser destruido con
 * destroy_storage_config
 */
t_storage_config *create_storage_config(const char *config_file_path);

/**
 * Elimina de la memoria la configuración de Storage y todos sus atributos
 *
 * @param storage_config La struct de configuración del storage
 */
void destroy_storage_config(t_storage_config *storage_config);

#endif
