#ifndef STORAGE_OPERATIONS_COMMIT_TAG_H_
#define STORAGE_OPERATIONS_COMMIT_TAG_H_

#include <commons/crypto.h>
#include "connection/serialization.h"
#include "connection/protocol.h"
#include "globals/globals.h"
#include "server/server.h"
#include "utils/filesystem_utils.h"

/**
 * Maneja la solicitud de COMMIT_TAG de un cliente.
 * 
 * @param package Paquete recibido con los parámetros del COMMIT_TAG.
 * @return t_package* Paquete de respuesta con el resultado de la operación,
 * o NULL si hay un error crítico (ej. fallo de memoria).
 */
t_package* handle_tag_commit_request(t_package *package);

/**
 * Ejecuta la lógica de commit para archivo:tag.
 * 
 * @param query_id ID único de la consulta.
 * @param name Nombre del archivo.
 * @param tag Etiqueta (versión) a comitear.
 * @return int Código de resultado de la operación (ej. SUCCESS, FILE_TAG_MISSING, -1 por error interno).
 */
int execute_tag_commit(uint32_t query_id, const char *name, const char *tag);


/**
 * Deserializa la solicitud de COMMIT_TAG del paquete entrante.
 * @note Asigna memoria para 'name' y 'tag' usando package_read_string(), 
 * por lo que el caller es responsable de liberarlos.
 * 
 * @param package Paquete de entrada a deserializar.
 * @param query_id Puntero a uint32_t donde se almacenará el ID de la query.
 * @param name Puntero a puntero (char**) donde se almacenará el nombre (malloc-ed).
 * @param tag Puntero a puntero (char**) donde se almacenará el tag (malloc-ed).
 * @return int 0 en caso de éxito, -1 en caso de error de deserialización.
 */
int deserialize_tag_commit_request(t_package *package, uint32_t *query_id, char **name, char **tag);

int deduplicate_blocks(uint32_t query_id, const char *name, const char *tag, t_file_metadata *metadata);

#endif