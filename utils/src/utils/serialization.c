#include "serialization.h"

t_buffer *buffer_create(size_t capacity) {
        t_buffer *new_buffer = malloc(sizeof(t_buffer));
        if (!new_buffer) return NULL;

        new_buffer->capacity = capacity;
        new_buffer->stream = malloc(capacity);
        new_buffer->offset = 0;

        return new_buffer;
}

void buffer_destroy(t_buffer *buffer) {
    if (!buffer) return;
    free(buffer->stream);
    free(buffer);
}

void buffer_write_uint8(t_buffer *buffer, uint8_t value) {
    if (buffer->size + value > buffer->capacity) return;

    memcpy(buffer->stream + buffer->size, &value, sizeof(uint8_t));
    buffer->size += sizeof(uint8_t);
}
