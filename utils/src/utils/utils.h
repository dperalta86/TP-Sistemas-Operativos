#ifndef UTILS_H
#define UTILS_H

#include <limits.h>
#include <unistd.h>
#include <commons/log.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <commons/string.h>
#include <sys/stat.h>

t_log* create_logger(char *directory, char *process_name, bool is_active_console, t_log_level log_level);

int log_set_level(t_log* logger, t_log_level new_log_level);
size_t strlcpy(char *dest, const char *src, size_t size);

char* get_stringified_array(char** array);

/**
 * Verifica si una ruta apunta a un archivo regular existente.
 * 
 * @param file_path Ruta completa al archivo.
 * @return bool true si la ruta es un archivo regular existente, false en caso contrario.
 */
bool regular_file_exists(char *file_path);

#endif