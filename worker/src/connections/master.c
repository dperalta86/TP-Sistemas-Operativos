#include "master.h"

static int send_request_and_wait_ack(int master_socket,
                                   t_package *request,
                                   t_master_op_code expected_response_code,
                                   const char *operation_name)
{
    t_log *logger = logger_get();

    if (!request)
    {
        log_error(logger, "Error al crear el paquete para %s", operation_name);
        return -1;
    }

    if (package_send(request, master_socket) != 0)
    {
        log_error(logger, "Error al enviar la solicitud de %s a Master", operation_name);
        package_destroy(request);
        return -1;
    }

    package_destroy(request);

    t_package *response = package_receive(master_socket);
    if (!response)
    {
        log_error(logger, "Error al recibir la respuesta de %s de Master", operation_name);
        return -1;
    }

    if (response->operation_code != expected_response_code)
    {
        log_error(logger, "Tipo de paquete inesperado para la respuesta de %s (esperado=%u, recibido=%u)",
                  operation_name, (unsigned)expected_response_code, (unsigned)response->operation_code);
        package_destroy(response);
        return -1;
    }

    package_destroy(response);
    return 0;
}

int handshake_with_master(const char *master_ip,
                          const char *master_port,
                          int worker_id)
{
    return handshake_with_server("Master",
                                 master_ip,
                                 master_port,
                                 OP_WORKER_HANDSHAKE_REQ,
                                 OP_WORKER_ACK,
                                 worker_id);
}

int end_query_in_master(int socket_master, int worker_id, int query_id)
{
    t_log *logger = logger_get();
    if (socket_master < 0)
    {
        log_error(logger, "Socket de Master inválido o worker_id nulo");
        return -1;
    }

    t_package *request = package_create_empty(OP_WORKER_END_QUERY);
    
    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_uint32(request, query_id))
    {
        int result = send_request_and_wait_ack(socket_master, request, 
                                            OP_WORKER_ACK, 
                                            "notificación de fin de query");
        if (result == 0)
        {
            log_debug(logger, "Notificación de fin de query enviada y confirmada por Master");
        }
        return result;
    }

    log_error(logger, "Error al preparar el paquete para fin de query");
    if (request) package_destroy(request);
    return -1;
}

int send_read_content_to_master(int socket_master, int query_id, void *data, size_t data_size, int worker_id)
{
    t_log *logger = logger_get();
    if (socket_master < 0)
    {
        log_error(logger, "Socket de Master inválido");
        return -1;
    }
    if (data == NULL || data_size == 0)
    {
        log_error(logger, "Datos nulos o tamaño cero para enviar a Master");
        return -1;
    }

    t_package *request = package_create_empty(OP_WORKER_READ_MESSAGE_REQ);
    
    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_uint32(request, query_id) &&
        package_add_data(request, data, data_size))
    {
        if (package_send(request, socket_master) == 0)
        {
            log_debug(logger, "Datos leídos enviados al Master (query_id=%d, worker_id=%d, size=%zu bytes)", 
                     query_id, worker_id, data_size);
            package_destroy(request);
            return 0;
        }
        log_error(logger, "Error al enviar los datos leidos al Master");
        package_destroy(request);
        return -1;
    }

    log_error(logger, "Error al preparar el paquete para enviar datos leidos");
    if (request) package_destroy(request);
    return -1;
}
