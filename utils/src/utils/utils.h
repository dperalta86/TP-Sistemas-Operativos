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

t_log* create_logger(char *directory, char *process_name, bool is_active_console, t_log_level log_level);

int log_set_level(t_log* logger, t_log_level new_log_level);

#endif