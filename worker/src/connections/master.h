#ifndef CONNECTION_MASTER_H
#define CONNECTION_MASTER_H

#include <connection/serialization.h>
#include <utils/logger.h>
#include <utils/client_socket.h>
#include <connection/protocol.h>
#include "common.h"

/**
 * Establece una conexión y realiza el handshake con el Master.
 * @param master_ip La IP del Master.
 * @param master_port El puerto del Master.
 * @param worker_id El ID del Worker.
 * @return El socket de la conexión con Master si el handshake fue exitoso, -1 en caso de error.
 */
int handshake_with_master(const char *master_ip, const char *master_port, char *worker_id);
int send_read_content_to_master(int socket_master, uint8_t *data, size_t data_size, int query_id, int worker_id);
int end_query_in_master(int socket_master, int worker_id, int query_id);

#endif
