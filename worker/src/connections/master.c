#include "master.h"

int handshake_with_master(const char *master_ip, const char *master_port)
{
    t_log *logger = logger_get();
    int client_socket_master = client_connect(master_ip, master_port);
    if (client_socket_master < 0)
    {
        log_error(logger, "## No se pudo establecer conexión con Master. IP=%s:%s", master_ip, master_port);
        return -1;
    }

    log_info(logger, "## Se establecio conexión con Master. IP=%s:%s", master_ip, master_port);

    t_buffer *buffer = buffer_create(sizeof(uint8_t));
    buffer_write_uint8(buffer, OP_WORKER_HANDSHAKE_REQ);
    t_package *request = package_create(OP_WORKER_HANDSHAKE_REQ, buffer);
    if (package_send(request, client_socket_master) < 0) {
        log_error(logger, "## No se pudo enviar el handshake al Master. IP=%s:%s", master_ip, master_port);
        buffer_destroy(buffer);
        free(request);
        close(client_socket_master);
        return -1;
    }
    package_destroy(request);

    t_package *response = package_receive(client_socket_master);
    if (response == NULL) {
        log_error(logger, "## No se pudo recibir el handshake del Master. IP=%s:%s", master_ip, master_port);
        close(client_socket_master);
        return -1;
    }

    if (response->operation_code != OP_WORKER_HANDSHAKE_RES) {
        log_error(logger, "## Handshake con Master fallido. IP=%s:%s", master_ip, master_port);
        package_destroy(response);
        close(client_socket_master);
        return -1;
    }

    log_info(logger, "## Handshake con Master exitoso. IP=%s:%s", master_ip, master_port);

    package_destroy(response);
    return client_socket_master;
}
