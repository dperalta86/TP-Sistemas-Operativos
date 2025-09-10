#ifndef WORKER_LOGGER_H
#define WORKER_LOGGER_H

#include <commons/log.h>
#include <stdbool.h>

/**
 * Inicializa un logger global en "<cwd>/worker.log"
 *
 * @param process_name Nombre del proceso
 * @param level_str Nivel del logger: "TRACE" | "DEBUG" | "INFO" | "WARNING" | "ERROR"
 * @param to_console Bandera para indicar si loggea en consola
 *
 * @return retorna 0 si es exitoso y -1 si hay un error.
 */
int logger_init(const char *process_name, const char *level_str, bool to_console);

// Devuelve el logger global (puede ser NULL si no se inicializó)

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
