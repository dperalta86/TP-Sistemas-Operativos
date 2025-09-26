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
int handshake_with_storage(const char *storage_ip, const char *storage_port, char *worker_id);

#endif
