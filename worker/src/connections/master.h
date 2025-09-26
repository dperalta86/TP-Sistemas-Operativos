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

#endif
