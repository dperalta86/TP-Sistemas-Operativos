#ifndef STORAGE_OPERATIONS_TRUNCATE_FILE_H_
#define STORAGE_OPERATIONS_TRUNCATE_FILE_H_

#include "connection/protocol.h"
#include "connection/serialization.h"
#include "globals/globals.h"
#include <stdint.h>

/**
 * Handler de protocolo para la operación TRUNCATE_FILE
 * Deserializa el package recibido y trunca el archivo especificado
 *
 * @param package Package recibido con query_id, nombre de archivo, tag y nuevo
 * tamaño
 * @return t_package* Package de respuesta con resultado de la operación, o NULL
 * en caso de error
 */
t_package *handle_truncate_file_op_package(t_package *package);

/**
 * Trunca un archivo existente a un tamaño nuevo
 * Si el tamaño nuevo es menor, libera bloques sobrantes
 * Si el tamaño nuevo es mayor, asigna nuevos bloques con hard links al bloque 0
 *
 * @param query_id ID de la query para logging
 * @param name Nombre del archivo a truncar
 * @param tag Tag del archivo a truncar
 * @param new_size_bytes Nuevo tamaño del archivo en bytes
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de éxito, valores negativos en caso de error
 *         -1: Error al abrir metadata.config
 *         -2: Error al crear hard links para expansión
 */
int truncate_file(uint32_t query_id, const char *name, const char *tag,
                  int new_size_bytes, const char *mount_point);

/**
 * Elimina un bloque lógico y libera el bloque físico asociado si ya no es
 * referenciado
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param name Nombre del archivo
 * @param tag Tag del archivo
 * @param logical_block_index Índice del bloque lógico a eliminar
 * @param physical_block_index Índice del bloque físico asociado
 * @param query_id ID de la query para logging
 * @return 0 en caso de éxito, valores negativos en caso de error
 *         -1: Error al eliminar el bloque lógico
 *         -2: Error al obtener estado del bloque físico
 *         -3: Error al liberar el bloque en el bitmap
 */
int delete_logical_block(const char *mount_point, const char *name,
                         const char *tag, int logical_block_index,
                         int physical_block_index, uint32_t query_id);

#endif
