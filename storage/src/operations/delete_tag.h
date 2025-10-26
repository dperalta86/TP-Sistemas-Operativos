#ifndef STORAGE_OPERATIONS_DELETE_TAG_H_
#define STORAGE_OPERATIONS_DELETE_TAG_H_

#include "connection/protocol.h"
#include "connection/serialization.h"
#include "globals/globals.h"
#include "server/server.h"
#include "utils/filesystem_utils.h"
#include <stdint.h>

/**
 * Elimina un file:tag, limpiando todos los physical blocks huérfanos.
 *
 * @param query_id ID de la query para logging
 * @param name Nombre del archivo
 * @param tag Tag del archivo a eliminar
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de éxito,
 *         FILE_TAG_MISSING si el tag no existe,
 *         -1 si se intenta eliminar un archivo protegido,
 *         -2 si falla la eliminación de bloques lógicos,
 *         -3 si falla la eliminación de la estructura de carpetas
 */
int delete_tag(uint32_t query_id, const char *name, const char *tag,
               const char *mount_point);

/**
 * Handlea el paquete de solicitud de eliminación de tag desde el Worker
 *
 * @param package Paquete recibido con los parámetros de la operación
 * @return Paquete de respuesta con el resultado o NULL si falla la
 * deserialización
 */
t_package *handle_delete_tag_op_package(t_package *package);

#endif
