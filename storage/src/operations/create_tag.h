#ifndef STORAGE_OPERATIONS_CREATE_TAG_H_
#define STORAGE_OPERATIONS_CREATE_TAG_H_

#include "connection/protocol.h"
#include "connection/serialization.h"
#include "globals/globals.h"
#include "server/server.h"
#include "utils/filesystem_utils.h"
#include "errors.h"
#include <inttypes.h>
#include <stdint.h>

/**
 * Crea un nuevo tag copiando todos los datos de un tag existente
 *
 * @param query_id ID de la query para logging
 * @param file_src Nombre del archivo origen
 * @param tag_src Tag origen
 * @param file_dst Nombre del archivo destino
 * @param tag_dst Tag destino
 * @return 0 en caso de éxito,
 *         -1 si el tag destino ya existe,
 *         -2 si falla la operación de copia,
 *         -3 si no se puede leer la metadata después de copiar,
 *         -4 si no se puede guardar la metadata actualizada
 */
int create_tag(uint32_t query_id, const char *file_src, const char *tag_src,
               const char *file_dst, const char *tag_dst);

/**
 * Handlea el paquete de solicitud de creación de tag desde el Worker
 *
 * @param package Paquete recibido con los parámetros de la operación
 * @return Paquete de respuesta con el resultado o NULL si falla la
 * deserialización
 */
t_package *handle_create_tag_op_package(t_package *package);

#endif
