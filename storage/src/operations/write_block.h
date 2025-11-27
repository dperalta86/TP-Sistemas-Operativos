#ifndef STORAGE_OPERATIONS_WRITE_BLOCK_H_
#define STORAGE_OPERATIONS_WRITE_BLOCK_H_

#include <stdio.h>
#include <commons/bitarray.h>
#include <inttypes.h>
#include "connection/serialization.h"
#include "connection/protocol.h"
#include "globals/globals.h"
#include "server/server.h"
#include "utils/filesystem_utils.h"
#include "file_locks.h"
#include "errors.h"

/**
 * Maneja la solicitud de operación WRITE BLOCK recibida desde un Worker.
 * Deserializa los datos, invoca la lógica principal de escritura de bloque y
 * arma el paquete de respuesta.
 * 
 * @param package El paquete serializado recibido del Worker.
 * @return t_package* Un paquete de respuesta indicando éxito (código 0) o un error de protocolo. 
 * Retorna NULL en caso de errores internos irrecuperables.
 */
t_package *handle_write_block_request(t_package *package);

/**
 * Implementa la lógica principal para la operación WRITE BLOCK del Storage.
 * Valida los permisos, gestiona el espacio libre (bitmap),
 * y realiza la reasignación de bloque físico y la escritura final de los datos en disco.
 *
 * @param name Nombre del archivo lógico.
 * @param tag Tag asociada al archivo.
 * @param query_id Identificador de la consulta o petición en curso (usado para logs).
 * @param block_number Número de bloque lógico a escribir.
 * @param block_content Contenido a escribir en el bloque físico correspondiente.
 *
 * @return int 0 si la operación fue exitosa, negativo si falla
 */
int execute_block_write(const char *name, const char *tag, uint32_t query_id, uint32_t block_number, const char *block_content);

/**
 * Escribe el contenido de un bloque en el disco, a través del hardlink lógico.
 * Asume que el bloque físico ya está apuntado por el hardlink lógico.
 * Abre el archivo en modo r+b y escribe el contenido.
 * 
 * @param query_id ID de la Query asociada a la operación (para logging).
 * @param file_name Nombre del archivo lógico.
 * @param tag Tag asociada al archivo.
 * @param block_number Número de bloque lógico a escribir.
 * @param block_content Contenido a escribir.
 * @return int 0 si la escritura es exitosa, -1 en caso de error de E/S.
 */
int write_to_logical_block(uint32_t query_id, const char *file_name, const char *tag, uint32_t block_number, const char *block_content);

/**
 * Crea un nuevo hardlink (en la ruta lógica) que apunta a un bloque físico.
 * 
 * @param query_id ID de la Query para logging.
 * @param name Nombre de archivo.
 * @param tag Tag de archivo.
 * @param logical_block Número de bloque lógico.
 * @param logical_block_path Ruta donde se creará el nuevo hardlink lógico.
 * @param bit_index Índice del bit (número de bloque físico) asignado.
 * @return int 0 si el hardlink se creó correctamente, -1 en caso de fallo.
 */
int create_new_hardlink(uint32_t query_id, const char *name, const char *tag, uint32_t logical_block, char *logical_block_path, ssize_t bit_index);

/**
 * Deserializa los datos necesarios para la operación WRITE BLOCK.
 * Extrae de forma segura el ID de Query, el File Name, el Tag, el número de bloque
 * y el contenido a escribir. Libera los punteros asignados (name, tag, block_content) 
 * si la deserialización falla a mitad de proceso.
 * 
 * @param package El paquete serializado recibido.
 * @param query_id Puntero donde se almacenará el ID de la Query.
 * @param name Puntero al puntero donde se almacenará el nombre del File (debe ser liberado).
 * @param tag Puntero al puntero donde se almacenará el Tag (debe ser liberado).
 * @param block_number Puntero donde se almacenará el número de bloque lógico.
 * @param block_content Puntero al puntero donde se almacenará el contenido (debe ser liberado).
 * @return int 0 si la deserialización es exitosa, -1 si falla.
 */
int deserialize_block_write_request(t_package *package, uint32_t *query_id, char **name, char **tag, uint32_t *block_number, char **block_content);
#endif