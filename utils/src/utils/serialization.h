#ifndef SERIALIZATION_UTILS_H
#define SERIALIZATION_UTILS_H

#include <stdlib.h>

typedef struct
{
    size_t size;  // Tamaño del buffer
    void *stream; // Contenido del buffer
    void *offset; // Desplazamiento dentro del buffer
} t_buffer;

typedef struct
{
    size_t operation_code; // Identificador de tipo de mensaje
    t_buffer *buffer;      // Mensaje serializado
} t_package;

/**
 * Crear un buffer
 * @param buffer_size Tamaño del stream del buffer
 * @return Un buffer
 */
t_buffer *buffer_create(size_t buffer_size);

/**
 * Libera el espacio de memoria del buffer
 * @param buffer El buffer que se quiere eliminar
 */
void buffer_destroy(t_buffer *buffer);

#endif
