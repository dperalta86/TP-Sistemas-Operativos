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
 *         -3: Error al asignar memoria para array de bloques
 *         -4: Error al guardar metadata
 */
int truncate_file(uint32_t query_id, const char *name, const char *tag,
                  int new_size_bytes, const char *mount_point);

#endif
