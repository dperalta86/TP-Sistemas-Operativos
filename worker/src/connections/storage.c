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

int get_block_size(int storage_socket, uint16_t *block_size) {
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_WORKER_GET_BLOCK_SIZE_REQ);
    if (!request) {
        log_error(logger, "Error al crear el paquete para solicitar el tamaño del bloque");
        return -1;
    }
    if(package_send(request, storage_socket) != 0) {
        log_error(logger, "Error al enviar la solicitud de tamaño del bloque al Storage");
        package_destroy(request);
        return -1;
    }
    package_destroy(request);
    t_package *response = package_receive(storage_socket);
    if (!response) {
        log_error(logger, "Error al recibir la respuesta del tamaño del bloque");
        return -1;
    }
    if (response->operation_code != STORAGE_OP_WORKER_GET_BLOCK_SIZE_RES) {
        log_error(logger, "Tipo de paquete inesperado para la respuesta del tamaño del bloque");
        package_destroy(response);
        return -1;
    }
    
    package_read_uint16(response, block_size);
    package_destroy(response);
    return 0;
}
