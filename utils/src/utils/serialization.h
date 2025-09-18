#ifndef SERIALIZATION_UTILS_H
#define SERIALIZATION_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <commons/string.h>
#include <arpa/inet.h>

typedef struct
{
    size_t size;   // Tamaño del buffer
    void *stream;  // Contenido del buffer
    size_t offset; // Desplazamiento dentro del buffer
} t_buffer;

typedef struct
{
    uint8_t operation_code; // Identificador de tipo de mensaje
    t_buffer *buffer;      // Mensaje serializado
} t_package;

/**
 * Crear un buffer
 * @param capacity Tamaño del stream del buffer
 * @return Un buffer
 */
t_buffer *buffer_create(size_t capacity);

/**
 * Libera el espacio de memoria del buffer
 * @param buffer El buffer que se quiere eliminar
 */
void buffer_destroy(t_buffer *buffer);
void buffer_reset_offset(t_buffer *buffer);

void buffer_write_uint8(t_buffer *buffer, uint8_t value);
void buffer_write_uint16(t_buffer *buffer, uint16_t value);
void buffer_write_uint32(t_buffer *buffer, uint32_t value);
void buffer_write_string(t_buffer *buffer, char *value);

uint8_t buffer_read_uint8(t_buffer *buffer);
uint16_t buffer_read_uint16(t_buffer *buffer);
uint32_t buffer_read_uint32(t_buffer *buffer);
char *buffer_read_string(t_buffer *buffer);

/**
 * Crea un paquete
 * @param operation_code Número que identifica al paquete
 * @param buffer Buffer que contiene los datos del paquete
 * @return Paquete creado o NULL si hubo algun error
 */
t_package *package_create(uint8_t operation_code, t_buffer *buffer); // TODO: Cuando sepamos los mensajes usar otra estructura para operation_code

/**
 * Libera la memoria asociada a un paquete
 * @param package Puntero al paquete a destruir
 */
void package_destroy(t_package *package);

/**
 * Envía un paquete a través de un socket
 * @param package Paquete a enviar (ya creado y cargado con datos)
 * @param socket Descriptor del socket por donde se enviará el paquete
 */
int package_send(t_package *package, int socket);

/**
 * Recibe un paquete desde un socket
 * @param socket Descriptor del socket desde el cual se recibirá el paquete
 * @return package Puntero al paquete donde se almacenarán los datos recibidos
 */
t_package *package_receive(int socket);

#endif
