#ifndef STORAGE_OPERATIONS_TRUNCATE_FILE_H_
#define STORAGE_OPERATIONS_TRUNCATE_FILE_H_

#include "connection/protocol.h"
#include "connection/serialization.h"
#include "globals/globals.h"

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

#endif
