#ifndef SERIALIZATION_UTILS_H
#define SERIALIZATION_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <commons/string.h>
#include <arpa/inet.h>


// Estructuras
typedef struct
{
    size_t size;   // Tamaño del buffer
    void *stream;  // Contenido del buffer
    size_t offset; // Desplazamiento dentro del buffer
    bool is_dynamic; // Indica si el buffer es dinámico o no
} t_buffer;

typedef struct
{
    uint8_t operation_code; // Identificador de tipo de mensaje
    t_buffer *buffer;      // Mensaje serializado
} t_package;

// CONSTANTES Y LÍMITES DE SEGURIDAD
#define MAX_STRING_LENGTH (1024 * 1024)    // 1MB máximo para strings
#define MAX_DATA_SIZE (10 * 1024 * 1024)   // 10MB máximo para datos binarios
#define MAX_BUFFER_SIZE (100 * 1024 * 1024) // 100MB máximo para buffer total



// Gestión del buffer
t_buffer *buffer_create(size_t size);
t_buffer *buffer_create_dynamic(void); // Buffer que crece automáticamente
void buffer_destroy(t_buffer *buffer);
void buffer_reset_offset(t_buffer *buffer);

// Funciones de escritura
bool buffer_write_uint8(t_buffer *buffer, uint8_t value);
bool buffer_write_int8(t_buffer *buffer, int8_t value);
bool buffer_write_uint16(t_buffer *buffer, uint16_t value);
bool buffer_write_uint32(t_buffer *buffer, uint32_t value);
bool buffer_write_string(t_buffer *buffer, const char *value);
bool buffer_write_data(t_buffer *buffer, const void *data, size_t data_size);

// Funciones de lectura
bool buffer_read_uint8(t_buffer *buffer, uint8_t *value);
bool buffer_read_int8(t_buffer *buffer, int8_t *value);
bool buffer_read_uint16(t_buffer *buffer, uint16_t *value);
bool buffer_read_uint32(t_buffer *buffer, uint32_t *value);
char *buffer_read_string(t_buffer *buffer);
void *buffer_read_data(t_buffer *buffer, size_t *data_size);

// Funciones de utilidad
size_t buffer_remaining_capacity(t_buffer *buffer);
size_t buffer_used_size(t_buffer *buffer);

// API básica
t_package *package_create(uint8_t operation_code, t_buffer *buffer);
void package_destroy(t_package *package);
int package_send(t_package *package, int socket);
t_package *package_receive(int socket);

// NUEVA API SIMPLIFICADA - Recomendada para usar
t_package *package_create_empty(uint8_t operation_code);
bool package_add_uint8(t_package *package, uint8_t value);
bool package_add_int8(t_package *package, int8_t value);
bool package_add_uint16(t_package *package, uint16_t value);
bool package_add_uint32(t_package *package, uint32_t value);
bool package_add_string(t_package *package, const char *value);
bool package_add_data(t_package *package, const void *data, size_t size);

// Macros para agregar structs
#define package_add_struct(pkg, struct_ptr) \
    package_add_data(pkg, struct_ptr, sizeof(*(struct_ptr)))

// Funciones de lectura simplificadas del paquete recibido
bool package_read_uint8(t_package *package, uint8_t *value);
bool package_read_int8(t_package *package, int8_t *value);
bool package_read_uint16(t_package *package, uint16_t *value);
bool package_read_uint32(t_package *package, uint32_t *value);
char *package_read_string(t_package *package);
void *package_read_data(t_package *package, size_t *data_size);

// Macro para leer structs
#define package_read_struct(pkg, struct_type) \
    ((struct_type*)package_read_data(pkg, NULL))

// Funciones de utilidad del paquete
void package_reset_read_offset(t_package *package);
size_t package_get_data_size(t_package *package);

// Funciones para calcular tamaños necesarios
size_t calculate_string_size(const char *str);
size_t calculate_data_size(size_t data_length);
size_t calculate_total_size(size_t num_fields, ...);

#endif
