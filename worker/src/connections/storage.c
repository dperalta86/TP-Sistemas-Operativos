#include "storage.h"

int handshake_with_storage(const char *storage_ip,
                           const char *storage_port,
                           char *worker_id)
{
    return handshake_with_server("Storage",
                                 storage_ip, storage_port,
                                 STORAGE_OP_WORKER_SEND_ID_REQ,
                                 STORAGE_OP_WORKER_SEND_ID_RES,
                                 worker_id);
}

int get_block_size(int storage_socket, int *block_size) {
    t_log *logger = get_logger();
    t_package *package_req = package_create(STORAGE_OP_WORKER_GET_BLOCK_SIZE_REQ, NULL);
    if (!package_req) {
        log_error(logger, "Error al crear el paquete para solicitar el tamaño del bloque");
        return -1;
    }
    package_send(package_req, storage_socket);
    package_destroy(package_req);
    t_package *package_res = package_receive(storage_socket);
    if (!package_res) {
        log_error(logger, "Error al recibir la respuesta del tamaño del bloque");
        return -1;
    }
    if (package_res->header.op_code != STORAGE_OP_WORKER_GET_BLOCK_SIZE_RES) {
        log_error(logger, "Tipo de paquete inesperado para la respuesta del tamaño del bloque");
        package_destroy(package_res);
        return -1;
    }
    
    memcpy(block_size, package_res->payload, sizeof(int));
    package_destroy(package_res);
    return 0;
}
