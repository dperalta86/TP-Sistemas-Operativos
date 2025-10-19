#ifndef CONNECTION_COMMON_H
#define CONNECTION_COMMON_H

#include <utils/logger.h>
#include <utils/client_socket.h>
#include <connection/protocol.h>
#include <commons/string.h>

/**
 * Establece una conexión y realiza el handshake con el servidor especificado.
 * @param server_name El nombre del servidor (para logs).
 * @param ip La IP del servidor.
 * @param port El puerto del servidor.
 * @param request_op El código de operación para el mensaje de solicitud.
 * @param expected_response_op El código de operación esperado en la respuesta.
 * @param worker_id El ID del worker
 * @return El socket de la conexión si el handshake fue exitoso, -1 en caso de error.
 */
int handshake_with_server(const char *server_name,
                          const char *ip,
                          const char *port,
                          uint8_t request_op,
                          uint8_t expected_response_op,
                          char *worker_id);

#endif
