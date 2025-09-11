#include "serialization.h"

t_buffer *buffer_create(size_t size)
{
    t_buffer *new_buffer = malloc(sizeof(t_buffer));
    if (!new_buffer)
        return NULL;

    new_buffer->size = size;
    new_buffer->stream = malloc(size);
    new_buffer->offset = 0;

    return new_buffer;
}

void buffer_destroy(t_buffer *buffer)
{
    if (!buffer)
        return;
    free(buffer->stream);
    free(buffer);
}

void buffer_write_uint8(t_buffer *buffer, uint8_t value)
{
    size_t remaining_capacity = buffer->size - buffer->offset;
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    if (remaining_capacity < sizeof(uint8_t))
        return;

    memcpy(next_position, &value, sizeof(uint8_t));
    buffer->offset += sizeof(uint8_t);
}

void buffer_write_uint16(t_buffer *buffer, uint16_t value)
{
    size_t remaining_capacity = buffer->size - buffer->offset;
    uint16_t *next_position = (uint16_t *)buffer->stream + buffer->offset;

    if (remaining_capacity < sizeof(uint16_t))
        return;

    memcpy(next_position, &value, sizeof(uint16_t));
    buffer->offset += sizeof(uint16_t);
}

void buffer_write_uint32(t_buffer *buffer, uint32_t value)
{
    size_t remaining_capacity = buffer->size - buffer->offset;
    uint32_t *next_position = (uint32_t *)buffer->stream + buffer->offset;

    if (remaining_capacity < sizeof(uint32_t))
        return;

    memcpy(next_position, &value, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);
}

void buffer_write_string(t_buffer *buffer, char *value)
{
    uint32_t str_length = string_length(value) + 1;
    size_t size_msg = sizeof(uint32_t) + str_length;
    size_t remaining_capacity = buffer->size - buffer->offset;
    char *next_position = (char *)buffer->stream + buffer->offset;

    if (remaining_capacity < size_msg)
        return;

    memcpy(next_position, &str_length, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);
    next_position = (char *)buffer->stream + buffer->offset;
    memcpy(next_position, value, str_length);
    buffer->offset += str_length;
}
