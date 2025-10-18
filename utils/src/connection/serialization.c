#include "serialization.h"
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

t_buffer *buffer_create(size_t size){
    if (size == 0) 
    {
        errno = EINVAL;
        return NULL;
    }
    
    t_buffer *new_buffer = malloc(sizeof(t_buffer));
    if (!new_buffer)
    {
        errno = ENOMEM;
        return NULL;
    }

    new_buffer->size = size;
    new_buffer->offset = 0;
    new_buffer->stream = malloc(size);
    if (!new_buffer->stream)
    {
        free(new_buffer);
        return NULL;
    }
    new_buffer->is_dynamic = false;
    memset(new_buffer->stream, 0, size);
    if (!new_buffer->stream)
    {
        free(new_buffer);
        return NULL;
    }


    return new_buffer;
}

// Crea un buffer dinámico con tamaño inicial pequeño
t_buffer *buffer_create_dynamic(void)
{
    const size_t initial_size = 256;
    
    t_buffer *new_buffer = malloc(sizeof(t_buffer));
    if (!new_buffer) 
    {
        errno = ENOMEM;
        return NULL;
    }

    new_buffer->stream = malloc(initial_size);
    if (!new_buffer->stream) 
    {
        free(new_buffer);
        errno = ENOMEM;
        return NULL;
    }
    
    new_buffer->size = initial_size;
    new_buffer->offset = 0;
    new_buffer->is_dynamic = true;
    
    memset(new_buffer->stream, 0, initial_size);

    return new_buffer;
}

bool buffer_expand(t_buffer *buffer, size_t required_size)
{
    if (!buffer || !buffer->is_dynamic) 
    {
        return false;
    }

    size_t new_size = buffer->size;
    while (new_size < buffer->offset + required_size) 
    {
        new_size *= 2; // Duplicar el tamaño
    }

    if (new_size > MAX_BUFFER_SIZE) 
    {
        return false;
    }

    void *new_stream = realloc(buffer->stream, new_size);
    if (!new_stream) 
    {
        return false;
    }

    // Inicializar nueva área con zeros
    memset((uint8_t*)new_stream + buffer->size, 0, new_size - buffer->size);
    
    buffer->stream = new_stream;
    buffer->size = new_size;
    
    return true;
}

void buffer_destroy(t_buffer *buffer)
{
    if (!buffer)
    {
        return;
    }
    if (buffer->stream)
    {
        free(buffer->stream);
    }
    free(buffer);
}

void buffer_reset_offset(t_buffer *buffer)
{
    if (!buffer)
    {
        return;
    }
    buffer->offset = 0;
}

// Función helper para validar capacidad y expandir si es dinamico
static bool buffer_has_capacity(t_buffer *buffer, size_t required_size)
{
    if (!buffer || !buffer->stream) 
    {
        return false;
    }
    
    // Si es dinámico y no tiene capacidad, intentar expandir
    if (buffer->is_dynamic && (buffer->size - buffer->offset) < required_size) {
        return buffer_expand(buffer, required_size);
    }
    
    return (buffer->size - buffer->offset) >= required_size;
}

// Función para verificar capacidad (sólo funciones de lectura)
static bool buffer_check_capacity(t_buffer *buffer, size_t required_size)
{
    if (!buffer || !buffer->stream) {
        return false;
    }
    return (buffer->size - buffer->offset) >= required_size;
}

bool buffer_write_uint8(t_buffer *buffer, uint8_t value)
{
    if (!buffer_has_capacity(buffer, sizeof(uint8_t)))
    {
        return false;
    }
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    memcpy(next_position, &value, sizeof(uint8_t));
    buffer->offset += sizeof(uint8_t);

    return true;
}

bool buffer_write_int8(t_buffer *buffer, int8_t value)
{
    if (!buffer_has_capacity(buffer, sizeof(int8_t)))
    {
        return false;
    }
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    memcpy(next_position, &value, sizeof(int8_t));
    buffer->offset += sizeof(int8_t);

    return true;
}

bool buffer_write_uint16(t_buffer *buffer, uint16_t value)
{
    if (!buffer_has_capacity(buffer, sizeof(uint16_t))) 
    {
        return false;
    }
    
    // Convertir a network byte order (big-endian)
    uint16_t net_value = htons(value);
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;

    memcpy(next_position, &net_value, sizeof(uint16_t));
    buffer->offset += sizeof(uint16_t);

    return true;
}


bool buffer_write_uint32(t_buffer *buffer, uint32_t value)
{
    if (!buffer_has_capacity(buffer, sizeof(uint32_t))) {
        return false;
    }

    // Convertir a network byte order (big-endian)
    uint32_t net_value = htonl(value);
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(next_position, &net_value, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);
    
    return true;
}

bool buffer_write_string(t_buffer *buffer, const char *value)
{
    if (!buffer || !value) {
        return false;
    }

    uint32_t str_length = strlen(value);
    size_t total_size = sizeof(uint32_t) + str_length;
    
    if (!buffer_has_capacity(buffer, total_size)) {
        return false;
    }

    // Escribir longitud en network byte order
    if (!buffer_write_uint32(buffer, str_length)) {
        return false;
    }

    // Escribir string (sin null terminator en el stream)
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(next_position, value, str_length);
    buffer->offset += str_length;
    
    return true;
}

// Función nueva: escribir datos binarios arbitrarios
bool buffer_write_data(t_buffer *buffer, const void *data, size_t data_size)
{
    if (!buffer || !data || data_size == 0) 
    {
        return false;
    }

    size_t total_size = sizeof(uint32_t) + data_size;
    
    if (!buffer_has_capacity(buffer, total_size)) 
    {
        return false;
    }

    // Escribir tamaño primero
    if (!buffer_write_uint32(buffer, data_size)) 
    {
        return false;
    }

    // Escribir datos
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(next_position, data, data_size);
    buffer->offset += data_size;
    
    return true;
}

bool buffer_read_uint8(t_buffer *buffer, uint8_t *value)
{
    if (!buffer || !value || !buffer_check_capacity(buffer, sizeof(uint8_t))) {
        return false;
    }

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(value, next_position, sizeof(uint8_t));
    buffer->offset += sizeof(uint8_t);

    return true;
}

bool buffer_read_int8(t_buffer *buffer, int8_t *value)
{
    if (!buffer || !value || !buffer_check_capacity(buffer, sizeof(int8_t))) {
        return false;
    }

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(value, next_position, sizeof(int8_t));
    buffer->offset += sizeof(int8_t);

    return true;
}

bool buffer_read_uint16(t_buffer *buffer, uint16_t *value)
{
    if (!buffer || !value || !buffer_check_capacity(buffer, sizeof(uint16_t))) {
        return false;
    }

    uint16_t net_value;
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(&net_value, next_position, sizeof(uint16_t));
    
    // Convertir de network byte order a host byte order
    *value = ntohs(net_value);
    buffer->offset += sizeof(uint16_t);
    
    return true;
}

bool buffer_read_uint32(t_buffer *buffer, uint32_t *value)
{       
    if (!buffer || !value || !buffer_check_capacity(buffer, sizeof(uint32_t))) {
        return false;
    }

    uint32_t net_value;
    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(&net_value, next_position, sizeof(uint32_t));
    
    // Convertir de network byte order a host byte order
    *value = ntohl(net_value);
    buffer->offset += sizeof(uint32_t);
    
    return true;
}

char *buffer_read_string(t_buffer *buffer)
{
    if (!buffer) {
        return NULL;
    }

    uint32_t length;
    if (!buffer_read_uint32(buffer, &length)) {
        return NULL;
    }

    // Validar longitud razonable para evitar ataques
    if (length > MAX_STRING_LENGTH || !buffer_check_capacity(buffer, length)) {
        // Revertir offset si no se puede leer
        buffer->offset -= sizeof(uint32_t);
        return NULL;
    }

    char *value = malloc(length + 1);
    if (!value) {
        buffer->offset -= sizeof(uint32_t);
        return NULL;
    }

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(value, next_position, length);
    value[length] = '\0'; // Null terminator
    buffer->offset += length;

    return value;
}

// Función nueva: leer datos binarios
void *buffer_read_data(t_buffer *buffer, size_t *data_size)
{
    if (!buffer || !data_size) {
        return NULL;
    }

    uint32_t size;
    if (!buffer_read_uint32(buffer, &size)) {
        return NULL;
    }

    // Validar tamaño razonable
    if (size > MAX_DATA_SIZE || !buffer_has_capacity(buffer, size)) {
        buffer->offset -= sizeof(uint32_t);
        return NULL;
    }

    void *data = malloc(size);
    if (!data) {
        buffer->offset -= sizeof(uint32_t);
        return NULL;
    }

    uint8_t *next_position = (uint8_t *)buffer->stream + buffer->offset;
    memcpy(data, next_position, size);
    buffer->offset += size;
    
    *data_size = size;
    return data;
}

// Funciones API
t_package *package_create(uint8_t operation_code, t_buffer *buffer)
{
    t_package *package = malloc(sizeof(t_package));
    if (!package)
    {
        return NULL;
    }

    package->operation_code = operation_code;
    if (!buffer) {
        free(package);
        return NULL;
    }
    package->buffer = buffer;
    return package;
}

t_package *package_create_empty(uint8_t operation_code)
{
    t_package *package = malloc(sizeof(t_package));
    if (!package) {
        return NULL;
    }
    
    package->operation_code = operation_code;
    package->buffer = buffer_create_dynamic(); // Buffer que crece automáticamente
    
    if (!package->buffer) {
        free(package);
        return NULL;
    }
    
    return package;
}


bool package_add_uint8(t_package *package, uint8_t value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_write_uint8(package->buffer, value);
}

bool package_add_int8(t_package *package, int8_t value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_write_int8(package->buffer, value);
}

bool package_add_uint16(t_package *package, uint16_t value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_write_uint16(package->buffer, value);
}

bool package_add_uint32(t_package *package, uint32_t value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_write_uint32(package->buffer, value);
}

bool package_add_string(t_package *package, const char *value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_write_string(package->buffer, value);
}

bool package_add_data(t_package *package, const void *data, size_t size)
{
    if (!package || !package->buffer || !data || size == 0) {
        return false;
    }
    return buffer_write_data(package->buffer, data, size);
}

// Funciones de lectura del paquete
bool package_read_uint8(t_package *package, uint8_t *value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_read_uint8(package->buffer, value);
}

bool package_read_int8(t_package *package, int8_t *value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_read_int8(package->buffer, value);
}

bool package_read_uint16(t_package *package, uint16_t *value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_read_uint16(package->buffer, value);
}

bool package_read_uint32(t_package *package, uint32_t *value)
{
    if (!package || !package->buffer) {
        return false;
    }
    return buffer_read_uint32(package->buffer, value);
}

char *package_read_string(t_package *package)
{
    if (!package || !package->buffer) {
        return NULL;
    }
    return buffer_read_string(package->buffer);
}

void *package_read_data(t_package *package, size_t *data_size)
{
    if (!package || !package->buffer) {
        return NULL;
    }
    return buffer_read_data(package->buffer, data_size);
}

void package_reset_read_offset(t_package *package)
{
    if (!package || !package->buffer) {
        return;
    }
    buffer_reset_offset(package->buffer);
}

size_t package_get_data_size(t_package *package)
{
    if (!package || !package->buffer) {
        return 0;
    }
    return buffer_used_size(package->buffer);
}

void package_destroy(t_package *package)
{
    if (!package)
    {
        return;
    }

    if (package->buffer)
    {
        buffer_destroy(package->buffer);
    }
    free(package);
}

int package_send(t_package *package, int socket)
{
    if (!package || !package->buffer || socket < 0) 
    {
        return -1;
    }

    // Calcular tamaño total del paquete serializado
    uint32_t buffer_size = (uint32_t)package->buffer->size;
    uint32_t total_size = sizeof(uint8_t) + sizeof(uint32_t) + buffer_size;
    
    void *serialized_package = malloc(total_size);
    if (!serialized_package) {
        return -1;
    }

    size_t offset = 0;

    // Serializar op_code
    memcpy(serialized_package + offset, &package->operation_code, sizeof(uint8_t));
    offset += sizeof(uint8_t);

    // Serializar tamaño de bloque
    uint32_t net_buffer_size = htonl(buffer_size);
    memcpy(serialized_package + offset, &net_buffer_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // Serializar buffer
    memcpy(serialized_package + offset, package->buffer->stream, buffer_size);

    // Enviar todo de una vez
    ssize_t bytes_sent = send(socket, serialized_package, total_size, MSG_NOSIGNAL);
    free(serialized_package);
    
    if (bytes_sent != (ssize_t)total_size) {
        return -1;
    }
    
    return 0;
}

// Funcion auxiliar para recibir datos
static ssize_t recv_all(int socket, void *buffer, size_t length)
{
    size_t total_received = 0;
    uint8_t *ptr = (uint8_t *)buffer;
    
    while (total_received < length) 
    {
        ssize_t received = recv(socket, ptr + total_received, 
                              length - total_received, 0);
        if (received <= 0) 
        {
            return received; // Error o conexión cerrada
        }
        total_received += received;
    }
    
    return total_received;
}


t_package *package_receive(int socket)
{
    if (socket < 0) {
        return NULL;
    }

    t_package *package = malloc(sizeof(t_package));
    if (!package) {
        return NULL;
    }

    // Recibir operation_code
    if (recv_all(socket, &package->operation_code, sizeof(uint8_t)) != sizeof(uint8_t)) {
        free(package);
        return NULL;
    }

    // Recibir tamaño del buffer
    uint32_t net_buffer_size;
    if (recv_all(socket, &net_buffer_size, sizeof(uint32_t)) != sizeof(uint32_t)) {
        free(package);
        return NULL;
    }
    
    uint32_t buffer_size = ntohl(net_buffer_size);
    
    // Validar tamaño razonable
    if (buffer_size > MAX_BUFFER_SIZE) {
        free(package);
        return NULL;
    }

    // Crear buffer
    package->buffer = buffer_create(buffer_size);
    if (!package->buffer) {
        free(package);
        return NULL;
    }

    // Recibir datos del buffer
    if (buffer_size > 0) {
        if (recv_all(socket, package->buffer->stream, buffer_size) != (ssize_t)buffer_size) {
            package_destroy(package);
            return NULL;
        }
    }

    return package;
}

// Funciones auxiliares en comun
// Funciones para calcular tamaños
size_t calculate_string_size(const char *str)
{
    return sizeof(uint32_t) + (str ? strlen(str) : 0);
}

size_t calculate_data_size(size_t data_length)
{
    return sizeof(uint32_t) + data_length;
}

// Función variádica para calcular tamaño total de múltiples campos
size_t calculate_total_size(size_t num_fields, ...)
{
    va_list args;
    va_start(args, num_fields);
    
    size_t total = 0;
    for (size_t i = 0; i < num_fields; i++) {
        total += va_arg(args, size_t);
    }
    
    va_end(args);
    return total;
}

// Función auxiliares del buffer
size_t buffer_remaining_capacity(t_buffer *buffer)
{
    if (!buffer) 
    {
        return 0;
    }
    return buffer->size - buffer->offset;
}

size_t buffer_used_size(t_buffer *buffer)
{
    if (!buffer) 
    {
        return 0;
    }
    return buffer->offset;
}