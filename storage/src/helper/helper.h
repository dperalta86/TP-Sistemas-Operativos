#ifndef STORAGE_HELPER_H_
#define STROAGE_HELPER_H_

#include <string.h>
#include <commons/log.h>

size_t strlcpy(char *dest, const char *src, size_t size);
t_log_level log_level_from_string(char* level);

#endif