#ifndef STORAGE_OPERATIONS_COMMIT_TAG_H_
#define STORAGE_OPERATIONS_COMMIT_TAG_H_

#include <commons/config.h>
#include <commons/crypto.h>
#include <dirent.h>
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
 * @return int 0 en caso de éxito, o un valor negativo en caso de error de deserialización.
 */
int deserialize_tag_commit_request(t_package *package, uint32_t *query_id, char **name, char **tag);

/**
 * Itera sobre los bloques lógicos de un archivo para realizar la deduplicación.
 * Calcula el hash MD5 de cada bloque, lo compara con el índice de hashes global
 * y reasigna hard links si encuentra un bloque duplicado.
 * 
 * @param query_id ID de consulta.
 * @param name Nombre del archivo.
 * @param tag Tag del archivo.
 * @param metadata Puntero al metadata del archivo.
 * @return int 0 en éxito, o un valor negativo en caso de error.
 */
int deduplicate_blocks(uint32_t query_id, const char *name, const char *tag, t_file_metadata *metadata);

/**
 * Lee el contenido de un bloque lógico dado por su ruta.
 * Abre y lee el archivo, llenando el buffer proporcionado. Si el tamaño leído es
 * menor al tamaño de bloque esperado, rellena el resto con ceros para el hashing.
 * 
 * @param query_id ID de consulta.
 * @param logical_block_path Ruta completa al bloque lógico.
 * @param block_size Tamaño estándar de bloque (en bytes).
 * @param read_buffer Buffer de destino para el contenido leído.
 * @return int 0 en éxito, o un valor negativo en caso de error.
 */
int read_block_content(uint32_t query_id, const char *logical_block_path,
                       uint32_t block_size, char *read_buffer);

/**
 * Determina el nombre del bloque físico (blockXXXX) al que apunta un bloque lógico.
 * Compara los i-nodes del hard link lógico con todos los bloques físicos.
 * 
 * @param query_id ID de consulta.
 * @param logical_block_path Ruta del bloque lógico (hard link).
 * @param physical_block_name Buffer de salida (tamaño PATH_MAX) donde se escribe el nombre del bloque físico.
 * @return int 0 en éxito, o un valor negativo en caso de error.
 */
int get_current_physical_block(uint32_t query_id,
                               const char *logical_block_path,
                               char *physical_block_name);

/**
 * Parsea el número de bloque a partir de la cadena de texto que lo identifica.
 * 
 * @param physical_block_id Cadena que identifica el bloque físico.
 * @return int32_t El número de bloque o -1 en caso de fallo de parseo.
 */
int32_t get_physical_block_number(const char *physical_block_id);

/**
 * Reasigna un bloque lógico para que apunte a un nuevo bloque físico.
 * Elimina el hard link del bloque lógico existente y crea un nuevo hard link
 * que apunta al bloque físico especificado.
 * 
 * @param query_id ID de consulta.
 * @param logical_block_path Ruta del bloque lógico a reasignar.
 * @param physical_block_name Nombre del bloque físico destino (ej: "block0042").
 * @return int 0 en éxito, o un valor negativo en caso de error.
 */
int update_logical_block_link(uint32_t query_id, const char *logical_block_path,
                              char *physical_block_name);

/**
 * Actualiza el estado de un bloque físico en el bitmap si es necesario.
 * Verifica cuántos hard links (referencias) apuntan al bloque físico. Si el
 * número de links es 1 (solo la referencia del propio bloque), lo marca como
 * libre en el bitmap.
 * 
 * @param query_id ID de consulta.
 * @param physical_block_name Nombre del bloque físico a verificar (ej: "block0042").
 * @return int 0 si no requiere liberación o se liberó correctamente, o un valor negativo en caso de error.
 */
int free_ph_block_if_unused(uint32_t query_id,
                                     char *physical_block_name);

#endif