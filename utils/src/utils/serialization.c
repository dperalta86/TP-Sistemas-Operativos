#include "serialization.h"

t_buffer *buffer_create(size_t size)
{
    t_buffer *new_buffer = malloc(sizeof(t_buffer));
    if (!new_buffer)
        return NULL;

    new_buffer->size = size;
    new_buffer->stream = malloc(size);
    if (!new_buffer->stream)
    {
        free(new_buffer);
        return NULL;
    }
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

    if (!value)
    {
        buffer->offset -= size_length;
        return NULL;
    }

    next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(value, next_position, length);
    value[length] = '\0'; // Esto es para que le agregue el terminador del string
    buffer->offset += length;

    return value;
}

t_package *package_create(size_t operation_code)
{
    t_package *package = malloc(sizeof(t_package));
    if (!package)
        return NULL;
    package->operation_code = operation_code;
    return package;
}

void package_destroy(t_package *package)
{
    if (!package)
        return;
    if (package->buffer != NULL)
        buffer_destroy(package->buffer);
    free(package);
}

void package_send(t_package *package, int socket)
{
    uint32_t serialized_package_size = sizeof(package->operation_code) + sizeof(size_t) + package->buffer->size;
    void *serialized_package = malloc(serialized_package_size);
    if (!serialized_package)
        return NULL;
    int offset = 0;

    memcpy(serialized_package + offset, &(package->operation_code), sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(serialized_package + offset, &(package->buffer->size), sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(serialized_package + offset, package->buffer->stream, package->buffer->size);

    send(socket, serialized_package, serialized_package_size, 0);

    free(serialized_package);
    package_destroy(package);
}

t_package *package_receive(int socket)
{
    t_package *package = malloc(sizeof(t_package));
    if (!package)
        return NULL;

    package->buffer = malloc(sizeof(t_buffer));
    if (!package->buffer)
    {
        free(package);
        return NULL;
    }

    if (recv(socket, &package->operation_code, sizeof package->operation_code, 0) <= 0) {
        free(package->buffer);
        free(package);
        return NULL;
    }
    if (recv(socket, &package->buffer->size, sizeof package->buffer->size, 0) <= 0) {
        free(package->buffer);
        free(package);
        return NULL;
    }

    package->buffer->stream = malloc(package->buffer->size);
    if (!package->buffer->stream) {
        free(package->buffer);
        free(package);
        return NULL;
    }

    if (recv(socket, package->buffer->stream, package->buffer->size, 0) <= 0) {
        free(package->buffer->stream);
        free(package->buffer);
        free(package);
        return NULL;
    }

    return package;
}
