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

void buffer_reset_offset(t_buffer *buffer)
{
    if (!buffer)
        return;
    buffer->offset = 0;
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
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    if (remaining_capacity < sizeof(uint16_t))
        return;

    memcpy(next_position, &value, sizeof(uint16_t));
    buffer->offset += sizeof(uint16_t);
}

void buffer_write_uint32(t_buffer *buffer, uint32_t value)
{
    size_t remaining_capacity = buffer->size - buffer->offset;
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    if (remaining_capacity < sizeof(uint32_t))
        return;

    memcpy(next_position, &value, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);
}

void buffer_write_string(t_buffer *buffer, char *value)
{
    uint32_t str_length = string_length(value);
    size_t size_msg = sizeof(uint32_t) + str_length;
    size_t remaining_capacity = buffer->size - buffer->offset;
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    if (remaining_capacity < size_msg)
        return;

    uint32_t net_len = htonl(str_length);

    memcpy(next_position, &net_len, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);

    next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(next_position, value, str_length);
    buffer->offset += str_length;
}


uint8_t buffer_read_uint8(t_buffer *buffer)
{
    uint8_t value = 0;
    size_t remaining_stream_size = buffer->size - buffer->offset;
    size_t value_size = sizeof(uint8_t);

    if (remaining_stream_size < value_size)
        return value;

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(&value, next_position, value_size);

    buffer->offset += value_size;

    return value;
}

uint16_t buffer_read_uint16(t_buffer *buffer)
{
    uint16_t value = 0;
    size_t remaining_stream_size = buffer->size - buffer->offset;
    size_t value_size = sizeof(uint16_t);

    if (remaining_stream_size < value_size)
        return value;

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(&value, next_position, value_size);

    buffer->offset += value_size;

    return value;
}

uint32_t buffer_read_uint32(t_buffer *buffer)
{
    uint32_t value = 0;
    size_t remaining_stream_size = buffer->size - buffer->offset;
    size_t value_size = sizeof(uint32_t);

    if (remaining_stream_size < value_size)
        return value;

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(&value, next_position, value_size);

    buffer->offset += value_size;

    return value;
}

char *buffer_read_string(t_buffer *buffer)
{
    size_t remaining_size = buffer->size - buffer->offset;
    size_t size_length = sizeof(uint32_t);
    uint32_t length = 0;
    char *value = NULL;

    if (buffer->size < buffer->offset)
        return NULL;

    if (remaining_size < size_length)
        return NULL;

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(&length, next_position, size_length);
    length = ntohl(length);

    buffer->offset += size_length;
    remaining_size = buffer->size - buffer->offset;

    if (remaining_size < length)
    {
        buffer->offset -= size_length;
        return NULL;
    }

    value = malloc(length + 1);

    next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(value, next_position, length);
    value[length] = '\0'; // Esto es para que le agregue el terminador del string
    buffer->offset += length;

    return value;
}
