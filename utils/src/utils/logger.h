#ifndef LOGGER_H
#define LOGGER_H

#include <commons/log.h>
#include <stdbool.h>

/**
 * Inicializa un logger global en "<cwd>/process_name.log"
 *
 * @param process_name Nombre del proceso (se utiliza para el nombre del proceso y el nombre de archivo)
 * @param log_level Nivel del logger: TRACE | DEBUG | INFO | WARNING | ERROR
 * @param to_console Bandera para indicar si loggea en consola
 *
 * @return retorna 0 si es exitoso y -1 si hay un error.
 */
int logger_init(const char *process_name, t_log_level log_level, bool to_console);

/**
 * Devuelve el logger global
 * 
 * @return t_log* o NULL si no se inicializo
 */
t_log *logger_get();

/**
 * Destruye el logger global
 */
void logger_destroy();

#endif
