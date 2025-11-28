#ifndef STORAGE_OPERATIONS_READ_BLOCK_H_
#define STORAGE_OPERATIONS_READ_BLOCK_H_

#include "connection/serialization.h"
#include "connection/protocol.h"
#include "globals/globals.h"
#include "server/server.h"

/**
 * Maneja la solicitud de operación READ BLOCK recibida desde un Worker.
 * Deserializa los parámetros (ID, nombre, tag y bloque), asigna el buffer de lectura, 
 * invoca la lógica principal para obtener el bloque y arma el paquete de respuesta.
 * 
 * @param package El paquete serializado recibido del Worker.
 * @return t_package* Un paquete de respuesta que contiene el status de la operación (0 o error)
 * y el contenido del bloque leído en caso de éxito. Retorna NULL en caso de errores 
 * irrecuperables.
 */
t_package *handle_read_block_request(t_package *package);

/**
 * Deserializa los campos requeridos para la lectura de un bloque desde el paquete de solicitud.
 * Los punteros 'name' y 'tag' son asignados dinámicamente.
 * 
 * @param package El paquete de solicitud entrante.
 * @param query_id Puntero donde se almacena el ID de la consulta.
 * @param name Puntero donde se almacena el puntero al nombre del archivo (debe ser liberado).
 * @param tag Puntero donde se almacena el puntero al tag del archivo (debe ser liberado).
 * @param block_number Puntero donde se almacena el número de bloque lógico.
 * @return int 0 si la deserialización es exitosa, o un valor negativo si falla.
 */
int deserialize_block_read_request(t_package *package, uint32_t *query_id, char **name, char **tag, uint32_t *block_number);

/**
 * Ejecuta la lógica de alto nivel para leer un bloque lógico.
 * Se encarga de: tomar el lock del archivo, validar la existencia del directorio y
 * la metadata, verificar que el número de bloque esté dentro del rango, y llamar
 * a la lectura física del bloque.
 * 
 * @param name Nombre del archivo a leer.
 * @param tag Tag del archivo.
 * @param query_id ID de la consulta.
 * @param block_number Número de bloque lógico.
 * @param read_buffer Puntero al buffer pre-asignado donde se guardará el contenido leído.
 * @return int 0 si la lectura fue exitosa, o un código de error negativo específico
 */
int execute_block_read(const char *name, const char *tag, uint32_t query_id, uint32_t block_number, void *read_buffer);

/**
 * Realiza la lectura física del bloque de datos desde el disco.
 * Abre el archivo binario, lee exactamente el tamaño del bloque en el buffer,
 * asegura el terminador nulo ('\0') para su posterior serialización como string,
 * y maneja las validaciones de lectura, EOF y cierre de archivo.
 * 
 * @param query_id ID de la consulta para logs.
 * @param file_name Nombre del archivo.
 * @param tag Tag del archivo.
 * @param block_number Número de bloque lógico.
 * @param read_buffer Puntero al buffer (debe ser BLOCK_SIZE + 1) donde se cargarán los datos.
 * @return int 0 si la lectura es exitosa, o un código de error negativo:
 * -1 (Fallo al abrir), -2 (Fallo I/O), -3 (EOF/Lectura parcial), -4 (Fallo al cerrar).
 */
int read_from_logical_block(uint32_t query_id, const char *file_name, const char *tag, uint32_t block_number, void *read_buffer);

#endif