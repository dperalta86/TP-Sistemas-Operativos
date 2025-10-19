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
    
    if(!package_read_uint16(response, block_size)) {
        log_error(logger, "Error al leer del buffer el tamaño de bloque");
        package_destroy(response);
        return -1;
    }
    
    package_destroy(response);
    return 0;
}

int fork_file_in_storage(int storage_socket, char *file_src, char *tag_src, char *file_dst, char *tag_dst){
    t_log *logger = logger_get();
    t_package *request_package = package_create_empty(STORAGE_OP_TAG_CREATE_REQ);
    if (!request_package) {
        log_error(logger, "Error al crear el paquete para solicitar el fork (TAG) de un archivo");
        return -1;
    }
    if (!package_add_string(request_package, file_src) ||
        !package_add_string(request_package, tag_src) ||
        !package_add_string(request_package, file_dst) ||
        !package_add_string(request_package, tag_dst)) {
        log_error(logger, "Error al agregar datos al buffer del package");
        package_destroy(request_package);
        return -1;
    }
    if(package_send(request_package, storage_socket) != 0) {
        log_error(logger, "Error al enviar la solicitud fork create (TAG) al Storage");
        package_destroy(request_package);
        return -1;
    }
    package_destroy(request_package);
    t_package *response_package = package_receive(storage_socket);
    if (!response_package) {
        log_error(logger, "Error al recibir la respuesta a TAG create del Storage");
        return -1;
    }
    if (response_package->operation_code != STORAGE_OP_TAG_CREATE_RES) {
        log_error(logger, "Tipo de paquete inesperado para la respuesta a TAG create");
        package_destroy(response_package);
        return -1;
    }
    
    package_destroy(response_package);
    
    // El log info de la instrucción (pedido en el enunciado) lo debería hacer el intreter.
    log_debug(logger, "Fork del archivo %s:%s a %s:%s realizado con éxito", file_src, tag_src, file_dst, tag_dst);
    
    return 0;
}

int commit_file_in_storage(int storage_socket, char *file, char *tag){
    t_log *logger = logger_get();
    t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
    if (!request_package) {
        log_error(logger, "Error al crear el paquete para solicitar el commit de un archivo");
        return -1;
    }
    if (!package_add_string(request_package, file) ||
        !package_add_string(request_package, tag)) {
        log_error(logger, "Error al agregar datos al buffer del package");
        package_destroy(request_package);
        return -1;
    }
    if(package_send(request_package, storage_socket) != 0) {
        log_error(logger, "Error al enviar la solicitud commit al Storage");
        package_destroy(request_package);
        return -1;
    }
    package_destroy(request_package);
    t_package *response_package = package_receive(storage_socket);
    if (!response_package) {
        log_error(logger, "Error al recibir la respuesta a commit del Storage");
        return -1;
    }
    if (response_package->operation_code != STORAGE_OP_TAG_COMMIT_RES) {
        log_error(logger, "Tipo de paquete inesperado para la respuesta a commit");
        package_destroy(response_package);
        return -1;
    }
    
    package_destroy(response_package);
    
    // Idem log info de fork.
    log_debug(logger, "Commit del archivo %s:%s realizado con éxito", file, tag);
    
    return 0;
}