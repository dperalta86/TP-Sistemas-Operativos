#ifndef SERIALIZATION_UTILS_H
#define SERIALIZATION_UTILS_H

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

#endif
