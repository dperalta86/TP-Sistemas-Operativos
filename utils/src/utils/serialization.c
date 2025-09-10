#include "serialization.h"

t_buffer *buffer_create(size_t buffer_size) {
    t_buffer *new_buffer = malloc(sizeof(t_buffer));
    if (!new_buffer) return NULL;

    new_buffer->size = buffer_size;
    new_buffer->stream = malloc(buffer_size);
    new_buffer->offset = 0;

    return new_buffer;
}

void buffer_destroy(t_buffer *buffer) {
    if (!buffer) return;
    free(buffer->stream);
    free(buffer);
}
