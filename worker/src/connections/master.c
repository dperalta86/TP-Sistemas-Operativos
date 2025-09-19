#include "master.h"

int handshake_with_master(const char *master_ip, const char *master_port)
{
    return handshake_with_server("Master", master_ip, master_port, OP_WORKER_HANDSHAKE_REQ, OP_WORKER_HANDSHAKE_RES);
}

int send_id_to_master(int master_socket, char *id)
{
    t_log *logger = logger_get();
    t_package *request = NULL;
    t_package *response = NULL;
    t_buffer *buffer = NULL;

    size_t id_length = string_length(id) + 1;

    buffer = buffer_create(id_length);
    buffer_write_string(buffer, id);

    request = package_create(OP_WORKER_SEND_ID_REQ, buffer);
    if (package_send(request, master_socket) < 0)
    {
        log_error(logger, "## No se pudo enviar el ID al Master");
        goto clean;
    }
    package_destroy(request);

    response = package_receive(master_socket);
    if (response == NULL)
    {
        log_error(logger, "## No se pudo recibir la confirmación del ID del Master");
        goto clean;
    }

    if (response->operation_code != OP_WORKER_SEND_ID_RES)
    {
        log_error(logger, "## Envío de ID al Master fallido");
        goto clean;
    }

    log_info(logger, "## Envío de ID al Master exitoso");
    log_debug(logger, "## Operacion recibida: %d", response->operation_code);

    package_destroy(response);
    return 0;
clean:
    if (request)
        package_destroy(request);
    if (response)
        package_destroy(response);
    return -1;
}
