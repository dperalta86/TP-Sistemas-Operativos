#include "helper/helper.h"

size_t strlcpy(char *dest, const char *source, size_t size) {
    if (size == 0) {
        return strlen(source);
    }

    strncpy(dest, source, size - 1);
    dest[size - 1] = '\0';

    return strlen(source);
}