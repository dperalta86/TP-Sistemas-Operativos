#include "helper/helper.h"

size_t strlcpy(char *dest, const char *source, size_t size) {
    if (size == 0) {
        return strlen(source);
    }

    strncpy(dest, source, size - 1);
    dest[size - 1] = '\0';

    return strlen(source);
}

t_log_level log_level_from_string(char* level) {
    if (level == NULL) {
        fprintf(stderr, "Error: El string de nivel de log es nulo.\n");
        return LOG_LEVEL_INFO; // Valor por defecto
    }

    if (strcmp(level, "TRACE") == 0) {
        return LOG_LEVEL_TRACE;
    }
    if (strcmp(level, "DEBUG") == 0) {
        return LOG_LEVEL_DEBUG;
    }
    if (strcmp(level, "INFO") == 0) {
        return LOG_LEVEL_INFO;
    }
    if (strcmp(level, "WARNING") == 0) {
        return LOG_LEVEL_WARNING;
    }
    if (strcmp(level, "ERROR") == 0) {
        return LOG_LEVEL_ERROR;
    }
    
    // Si el string no coincide con ningún nivel conocido
    fprintf(stderr, "Advertencia: Nivel de log desconocido '%s'. Usando INFO por defecto.\n", level);
    return LOG_LEVEL_INFO;
}