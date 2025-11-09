#ifndef CONNECTION_STORAGE_H
#define CONNECTION_STORAGE_H

#include <connection/serialization.h>
#include <utils/logger.h>
#include <utils/client_socket.h>
#include <connection/protocol.h>
#include "common.h"

/**
 * Establece una conexión y realiza el handshake con el Storage.
 * @param storage_ip La IP del Storage.
 * @param storage_port El puerto del Storage.
 * @param worker_id El ID del Worker.
 * @return El socket de la conexión con Storage si el handshake fue exitoso, -1 en caso de error.
 */
int handshake_with_storage(const char *storage_ip, const char *storage_port, int worker_id);

/**
 * Consulta al Storage por el tamaño del block.
 * @param storage_socket El socket de la conexión con el Storage.
 * @param block_size Puntero donde se almacenará el tamaño del block.
 * @param worker_id El ID del Worker.
 * @return 0 si la operación fue exitosa, -1 en caso de error.
 */
int get_block_size(int storage_socket, uint16_t *block_size, int worker_id);

int read_block_from_storage(int storage_socket, char *file, char *tag, uint32_t block_number, void **data, size_t *size, int worker_id);
int create_file_in_storage(int storage_socket, int worker_id, char *file, char *tag);
int truncate_file_in_storage(int storage_socket, char *file, char *tag, size_t size, int worker_id);

/**
 * Realiza un fork (instrucción TAG) de un archivo en el Storage.
 * @param storage_socket El socket de la conexión con el Storage.
 * @param file_src El nombre del archivo fuente.
 * @param tag_src Tag del archivo fuente.
 * @param file_dst El nombre del archivo destino.
 * @param tag_dst Tag del archivo destino.
 * @param worker_id El ID del Worker.
 * @return 0 si la operación fue exitosa, -1 en caso de error.
 */
int fork_file_in_storage(int storage_socket, char *file_src, char *tag_src, char *file_dst, char *tag_dst, int worker_id);
int commit_file_in_storage(int storage_socket, char *file, char *tag, int worker_id);
int delete_file_in_storage(int storage_socket, char *file, char *tag, int worker_id);
int write_block_to_storage(int storage_socket, char *file, char *tag, uint32_t block_number, void *data, size_t size, int worker_id);

#endif
