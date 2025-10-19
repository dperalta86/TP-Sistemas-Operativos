#ifndef STORAGE_OPERATIONS_CREATE_FILE_H_
#define STORAGE_OPERATIONS_CREATE_FILE_H_

#include "connection/protocol.h"
#include "connection/serialization.h"
#include "globals/globals.h"
#include "server/server.h"
#include <commons/log.h>

/**
 * Handler de protocolo para la operación CREATE_FILE
 * Deserializa el package recibido y crea un archivo con tag en el filesystem
 *
 * @param package Package recibido con query_id, nombre de archivo y tag
 * @return t_package* Package de respuesta con resultado de la operación, o NULL
 * en caso de error
 */
t_package *create_file(t_package *package);

/**
 * Crea un nuevo File con el Tag especificado en el filesystem
 *
 * @param query_id ID de la query que solicita la creación
 * @param name Nombre del File a crear
 * @param tag Tag inicial del File
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de éxito, -1 si falla la creación de carpetas, -2 si falla
 * la creación de metadata
 */
int _create_file(uint32_t query_id, const char *name, const char *tag,
                 const char *mount_point);

#endif
