#include "master.h"

int handshake_with_master(const char *master_ip,
                          const char *master_port,
                          char *worker_id)
{
    return handshake_with_server("Master",
                                 master_ip,
                                 master_port,
                                 OP_WORKER_HANDSHAKE_REQ,
                                 OP_WORKER_ACK,
                                 worker_id);
}

int end_query_in_master(int socket_master, int worker_id, int query_id){
    t_log *logger = logger_get();
    if (socket_master < 0) {
        log_error(logger, "Socket de Master inválido o worker_id nulo");
        return -1;
    }

    t_package *request_package = package_create_empty(OP_WORKER_END_QUERY);
    if (!request_package) {
        log_error(logger, "Error al crear el paquete para notificar fin de query al Master");
        return -1;
    }

    if (!package_add_uint32(request_package, worker_id) ||
        !package_add_uint32(request_package, query_id)) {
        log_error(logger, "Error al agregar datos al buffer del package");
        package_destroy(request_package);
        return -1;
    }
    if(package_send(request_package, socket_master) != 0) {
        log_error(logger, "Error al enviar la notificación de fin de query al Master");
        package_destroy(request_package);
        return -1;
    }

    // Esperar confirmación del Master
    // Utilizo el mismo operation code que en el handshake para dar el OK
    t_package *response = package_receive(socket_master);
    if (!response) {
        log_error(logger, "Error al recibir la confirmación de fin de query del Master");
        package_destroy(request_package);
        return -1;
    }
    if (response->operation_code != OP_WORKER_ACK) {
        log_error(logger, "Confirmación de fin de query inválida del Master (op=%u, esperado=%u)",
                  (unsigned)response->operation_code, (unsigned)OP_WORKER_ACK);
        package_destroy(request_package);
        package_destroy(response);
        return -1;
    }

    log_debug(logger, "Notificación de fin de query enviada y confirmada por Master");

    // Libero memoria
    if(request_package)
    {
        package_destroy(request_package);
    }
    if(response)
    {
        package_destroy(response);
    }


    return 0;
}
