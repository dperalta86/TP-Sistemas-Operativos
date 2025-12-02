#include "storage.h"
#include "worker.h"
#include <string.h>

// Versión mejorada de send_request_and_wait_ack que maneja errores de Storage
static int  send_request_and_wait_ack_with_error_handling(int storage_socket,
                                                         int master_socket,
                                                         t_package *request,
                                                         t_storage_op_code expected_response_code,
                                                         const char *operation_name, 
                                                         int query_id)
{
    t_log *logger = logger_get();

    if (!request)
    {
        log_error(logger, "[send_request_and_wait_ack_with_error_handling] Error al crear el paquete para %s", operation_name);
        return -1;
    }

    if (package_send(request, storage_socket) != 0)
    {
        log_error(logger, "[send_request_and_wait_ack_with_error_handling] Error al enviar la solicitud de %s al Storage", operation_name);
        package_destroy(request);
        return -1;
    }

    package_destroy(request);

        t_package *storage_response = package_receive(storage_socket);
        if (!storage_response) {
            log_error(logger, "Error al recibir la respuesta de creación de archivo del Storage");
            return -1;
        }
        if (storage_response->operation_code == expected_response_code) {
            log_debug(logger, "Recibo ACK por parte de storage para la operación %s", operation_name);
        } else if (storage_response->operation_code == STORAGE_OP_ERROR) {
            log_error(logger, "Storage reportó error: %s", operation_name);
            handler_error_from_storage(storage_response, master_socket, query_id);
            return -1;
        } else {
            log_error(logger, "Tipo de paquete inesperado para la respuesta de creación de archivo...");
            return -1;
        }

        package_destroy(storage_response);
    return 0;
}

static int send_request_and_wait_ack(int storage_socket,
                                     t_package *request,
                                     t_storage_op_code expected_response_code,
                                     const char *operation_name, int worker_id)
{
    t_log *logger = logger_get();

    if (!request)
    {
        log_error(logger, "Error al crear el paquete para %s", operation_name);
        return -1;
    }

    if (package_send(request, storage_socket) != 0)
    {
        log_error(logger, "Error al enviar la solicitud de %s al Storage", operation_name);
        package_destroy(request);
        return -1;
    }

    package_destroy(request);

    t_package *response = package_receive(storage_socket);
    if (!response)
    {
        log_error(logger, "Error al recibir la respuesta de %s del Storage", operation_name);
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

int handshake_with_storage(const char *storage_ip,
                           const char *storage_port,
                           int worker_id)
{

    return handshake_with_server("Storage",
                                 storage_ip, storage_port,
                                 STORAGE_OP_WORKER_SEND_ID_REQ,
                                 STORAGE_OP_WORKER_SEND_ID_RES,
                                 worker_id);
}

int get_block_size(int storage_socket, uint16_t *block_size, int worker_id)
{
    t_log *logger = logger_get();

    t_package *request = package_create_empty(STORAGE_OP_WORKER_GET_BLOCK_SIZE_REQ);
    if (!request)
    {
        log_error(logger, "Error al crear el paquete para solicitar el tamaño del bloque");
        return -1;
    }

    if (!package_add_uint32(request, worker_id))
    {
        log_error(logger, "Error al agregar worker_id al paquete");
        package_destroy(request);
        return -1;
    }

    if (package_send(request, storage_socket) != 0)
    {
        log_error(logger, "Error al enviar la solicitud de tamaño del bloque al Storage");
        package_destroy(request);
        return -1;
    }
    package_destroy(request);

    t_package *response = package_receive(storage_socket);
    if (!response)
    {
        log_error(logger, "Error al recibir la respuesta del tamaño del bloque");
        return -1;
    }

    if (response->operation_code != STORAGE_OP_WORKER_GET_BLOCK_SIZE_RES)
    {
        log_error(logger, "Tipo de paquete inesperado para la respuesta del tamaño del bloque");
        package_destroy(response);
        return -1;
    }

    if (!package_read_uint16(response, block_size))
    {
        log_error(logger, "Error al leer del buffer el tamaño de bloque");
        package_destroy(response);
        return -1;
    }

    package_destroy(response);
    return 0;
}

int read_block_from_storage(int storage_socket, int master_socket, char *file, char *tag, uint32_t block_number, void **data, size_t *size, int query_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);

    if (!request)
    {
        log_error(logger, "Error al crear el paquete para lectura de bloque");
        return -1;
    }

    if (!package_add_uint32(request, query_id) ||
        !package_add_string(request, file) ||
        !package_add_string(request, tag) ||
        !package_add_uint32(request, block_number))
    {
        log_error(logger, "Error al agregar datos al paquete para lectura de bloque");
        package_destroy(request);
        return -1;
    }

        if (package_send(request, storage_socket) != 0)
    {
        log_error(logger, "[send_request_and_wait_ack_with_error_handling] Error al enviar la solicitud de lectura de bloque al Storage");
        package_destroy(request);
        return -1;
    }

    package_destroy(request);

    t_package *storage_response = package_receive(storage_socket);
    if (!storage_response) {
        log_error(logger, "Error al recibir la respuesta de creación de archivo del Storage");
        return -1;
    }
    if (storage_response->operation_code == STORAGE_OP_BLOCK_READ_RES) {
        log_debug(logger, "Recibo ACK por parte de storage para la operación lectura de bloque");
    } else if (storage_response->operation_code == STORAGE_OP_ERROR) {
        log_error(logger, "Storage reportó error: lectura de bloque");
        handler_error_from_storage(storage_response, master_socket, query_id);
        return -1;
    } else {
        log_error(logger, "Tipo de paquete inesperado para la respuesta de creación de archivo...");
        return -1;
    }

    uint32_t data_size = 0;
    if (!package_read_uint32(storage_response, &data_size))
    {
        log_error(logger, "Error al leer el tamaño de los datos del bloque");
        package_destroy(storage_response);
        return -1;
    }

    size_t received_data_size;
    void *received_data = package_read_data(storage_response, &received_data_size);
    if (!received_data || received_data_size != data_size)
    {
        log_error(logger, "Error al leer los datos del bloque o tamaño inconsistente");
        package_destroy(storage_response);
        return -1;
    }

    *data = malloc(data_size);
    if (!*data)
    {
        log_error(logger, "Error al reservar memoria para los datos del bloque");
        package_destroy(storage_response);
        return -1;
    }

    memcpy(*data, received_data, data_size);
    *size = data_size;
    package_destroy(storage_response);

    log_debug(logger, "Lectura del bloque %u del archivo %s:%s realizada con éxito (%u bytes)",
              block_number, file, tag, data_size);

    return 0;
}

int create_file_in_storage(int storage_socket, int master_socket, int worker_id, char *file, char *tag)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_FILE_CREATE_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag))
    {
    return send_request_and_wait_ack_with_error_handling(storage_socket,
                                                         master_socket,
                                                         request,
                                                         STORAGE_OP_FILE_CREATE_RES,
                                                         "create archivo", 
                                                         worker_id);
    }

    log_error(logger, "Error al preparar el paquete para crear archivo");
    if (request)
        package_destroy(request);
    return -1;
}

int truncate_file_in_storage(int storage_socket, int master_socket, char *file, char *tag, size_t size, int query_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_FILE_TRUNCATE_REQ);

    if (request &&
        package_add_uint32(request, query_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag) &&
        package_add_uint32(request, (uint32_t)size))
    {

    return send_request_and_wait_ack_with_error_handling(storage_socket,
                                                         master_socket,
                                                         request,
                                                         STORAGE_OP_FILE_TRUNCATE_RES,
                                                         "truncate archivo", 
                                                         query_id);
    }
    
    log_error(logger, "Error al preparar el paquete para truncar archivo");
    if (request)
        package_destroy(request);
    return -1;
}

int fork_file_in_storage(int storage_socket, int master_socket, char *file_src, char *tag_src, char *file_dst, char *tag_dst, int query_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_TAG_CREATE_REQ);

    if (request &&
        package_add_uint32(request, query_id) &&
        package_add_string(request, file_src) &&
        package_add_string(request, tag_src) &&
        package_add_string(request, file_dst) &&
        package_add_string(request, tag_dst))
    {
    
    return send_request_and_wait_ack_with_error_handling(storage_socket,
                                                        master_socket,
                                                        request,
                                                        STORAGE_OP_TAG_CREATE_RES,
                                                        "fork (TAG) de archivo", 
                                                        query_id);
    }

    log_error(logger, "Error al preparar el paquete para fork");
    if (request)
        package_destroy(request);
    return -1;
}

int commit_file_in_storage(int storage_socket, int master_socket,char *file, char *tag, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag))
    {

    return send_request_and_wait_ack_with_error_handling(storage_socket,
                                                        master_socket,
                                                        request,
                                                        STORAGE_OP_TAG_COMMIT_RES,
                                                        "truncate archivo", 
                                                        worker_id);
    }

    log_error(logger, "[commit_file_in_storage] Error al preparar el paquete para commit");
    if (request)
        package_destroy(request);
    return -1;
}

int write_block_to_storage(int storage_socket, int master_socket, char *file, char *tag, uint32_t block_number, void *data, size_t size, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);

    if (!request)
    {
        log_error(logger, "Error al crear el paquete para escritura de bloque");
        return -1;
    }

    if (!package_add_uint32(request, worker_id) ||
        !package_add_string(request, file) ||
        !package_add_string(request, tag) ||
        !package_add_uint32(request, block_number) ||
        !package_add_data(request, data, size))
    {
        log_error(logger, "Error al agregar datos al paquete para escritura de bloque");
        package_destroy(request);
        return -1;
    }

    if (package_send(request, storage_socket) != 0)
    {
        log_error(logger, "Error al enviar la solicitud de escritura de bloque al Storage");
        package_destroy(request);
        return -1;
    }
    package_destroy(request);

    t_package *response = package_receive(storage_socket);
    if (!response)
    {
        log_error(logger, "Error al recibir la respuesta de escritura de bloque del Storage");
        return -1;
    }

    // Verificar si Storage reportó un error
    if (response->operation_code == STORAGE_OP_ERROR)
    {
        log_error(logger, "Storage reportó error en escritura de bloque del archivo %s:%s", file, tag);
        handler_error_from_storage(response, master_socket, worker_id);
        package_destroy(response);
        return -1;
    }

    if (response->operation_code != STORAGE_OP_BLOCK_WRITE_RES)
    {
        log_error(logger, "Tipo de paquete inesperado para la respuesta de escritura de bloque (esperado=%u, recibido=%u)",
                  (unsigned)STORAGE_OP_BLOCK_WRITE_RES, (unsigned)response->operation_code);
        package_destroy(response);
        return -1;
    }

    package_destroy(response);

    log_debug(logger, "Escritura del bloque %u del archivo %s:%s realizada con éxito", block_number, file, tag);

    return 0;
}

int delete_file_in_storage(int storage_socket, int master_socket, char *file, char *tag, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_TAG_DELETE_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag))
    {

        int result = send_request_and_wait_ack(storage_socket, request,
                                               STORAGE_OP_TAG_DELETE_RES,
                                               "borrado de archivo", worker_id);
        if (result == 0)
        {
            log_debug(logger, "Borrado del archivo %s:%s realizado con éxito", file, tag);
        }
        return result;
    }

    log_error(logger, "Error al preparar el paquete para borrado de archivo");
    if (request)
        package_destroy(request);
    return -1;
}

void handler_error_from_storage(t_package *result, int master_socket, int query_id){
    t_log *logger = logger_get();

    if (!result || !logger) {
        printf("Paquete de error inválido o logger no disponible\n");
        if (logger) {
            log_error(logger, "[handler_error_from_storage] Paquete de error inválido");
        }
        return;
    }

    buffer_reset_offset(result->buffer);
    uint32_t qid_from_package;
    if (!package_read_uint32(result, &qid_from_package)) {
        log_error(logger, "[handler_error_from_storage] Error al leer query_id del paquete de error del Storage");
        return;
    }
    query_id = qid_from_package;  // Usar el query_id que vino del paquete

    char *error_message = package_read_string(result);
    if (!error_message)
    {
        log_error(logger, "[handler_error_from_storage] Error al leer el mensaje de error del paquete de error del Storage");
        return;
    }

    log_error(logger, "## Storage Error - Query ID: %u - %s", query_id, error_message);

    // Enviar notificación de error al Master
    t_package *error_package = package_create_empty(STORAGE_OP_ERROR);
    if (!error_package) {
        log_error(logger, "[handler_error_from_storage] Error al crear paquete de error para Master");
        free(error_message);
        return;
    }

    if (!package_add_uint32(error_package, query_id) || !package_add_string(error_package, error_message)) {
        log_error(logger, "[handler_error_from_storage] Error al agregar datos al paquete de error para Master");
        package_destroy(error_package);
        free(error_message);
        return;
    }

    if (package_send(error_package, master_socket) != 0) {
        log_error(logger, "[handler_error_from_storage] Error al enviar paquete de error al Master para Query ID=%u", query_id);
    } else {
        log_info(logger, "[handler_error_from_storage] Notificación de error enviada a Master para Query ID=%u", query_id);
    }

    package_destroy(error_package);
    free(error_message);
    return;
}
