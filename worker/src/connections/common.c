#include "common.h"
#include "connection/protocol.h"
#include "connection/serialization.h"

int handshake_with_server(const char *server_name,
                          const char *ip,
                          const char *port,
                          uint8_t request_op,
                          uint8_t expected_response_op,
                          char *worker_id)
{
    t_log *logger = logger_get();
    t_package *request = NULL;
    t_package *response = NULL;
    int socket = -1;

    socket = client_connect(ip, port);
    if (socket < 0)
    {
        log_error(logger, "## No se pudo establecer conexión con %s. IP=%s:%s", server_name, ip, port);
        goto clean;
    }
    log_info(logger, "## Se estableció conexión con %s. IP=%s:%s", server_name, ip, port);

    request = package_create_empty(request_op);
    if (!request)
    {
        log_error(logger, "## No se pudo crear package para %s", server_name);
        goto clean;
    }

    if(!package_add_string(request, worker_id))
    {
        log_error(logger, "## No se pudo agregar worker_id al package para %s", server_name);
        goto clean;
    }
    
    if (package_send(request, socket) < 0)
    {
        log_error(logger, "## No se pudo enviar el handshake a %s", server_name);
        goto clean;
    }
    package_destroy(request);
    request = NULL;

    response = package_receive(socket);
    if (!response)
    {
        log_error(logger, "## No se pudo recibir el handshake de %s", server_name);
        goto clean;
    }

    if (response->operation_code != expected_response_op)
    {
        log_error(logger, "## Handshake con %s fallido (op=%u, esperado=%u)",
                  server_name, (unsigned)response->operation_code, (unsigned)expected_response_op);
        goto clean;
    }

    log_info(logger, "## Handshake con %s exitoso", server_name);
    log_debug(logger, "## Operación recibida: %u", (unsigned)response->operation_code);

    package_destroy(response);
    return socket;

clean:
    if (request)
        package_destroy(request);
    if (response)
        package_destroy(response);
    if (socket >= 0)
        close(socket);
    return -1;
}
