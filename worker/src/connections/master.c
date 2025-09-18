#include "master.h"

int handshake_with_master(const char *master_ip, const char *master_port)
{
    t_log *logger = logger_get();
    t_package *request = NULL;
    t_package *response = NULL;
    t_buffer *buffer = NULL;
    int client_socket_master = -1;

    client_socket_master = client_connect(master_ip, master_port);
    if (client_socket_master < 0)
    {
        log_error(logger, "## No se pudo establecer conexión con Master. IP=%s:%s", master_ip, master_port);
        goto clean;
    }

    log_info(logger, "## Se establecio conexión con Master. IP=%s:%s", master_ip, master_port);

    buffer = buffer_create(sizeof(uint8_t));
    buffer_write_uint8(buffer, OP_WORKER_HANDSHAKE_REQ);
    request = package_create(OP_WORKER_HANDSHAKE_REQ, buffer);
    if (package_send(request, client_socket_master) < 0) {
        log_error(logger, "## No se pudo enviar el handshake al Master");
        goto clean;
    }
    package_destroy(request);

    response = package_receive(client_socket_master);
    if (response == NULL) {
        log_error(logger, "## No se pudo recibir el handshake del Master");
        goto clean;
    }

    if (response->operation_code != OP_WORKER_HANDSHAKE_RES) {
        log_error(logger, "## Handshake con Master fallido");
        goto clean;
    }

    log_info(logger, "## Handshake con Master exitoso");
    log_debug(logger, "## Operacion recibida: %d", response->operation_code);

    package_destroy(response);
    return client_socket_master;
clean:
    if (request) package_destroy(request);
    if (response) package_destroy(response);
    if (client_socket_master >= 0) close(client_socket_master);
    return -1;
}
