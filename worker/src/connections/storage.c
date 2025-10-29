#include "storage.h"
#include <string.h>

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

int read_block_from_storage(int storage_socket, char *file, char *tag, uint32_t block_number, void **data, size_t *size, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_BLOCK_READ_REQ);

    if (!request)
    {
        log_error(logger, "Error al crear el paquete para lectura de bloque");
        return -1;
    }

    if (!package_add_uint32(request, worker_id) ||
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
        log_error(logger, "Error al enviar la solicitud de lectura de bloque al Storage");
        package_destroy(request);
        return -1;
    }
    package_destroy(request);

    t_package *response = package_receive(storage_socket);
    if (!response)
    {
        log_error(logger, "Error al recibir la respuesta de lectura de bloque del Storage");
        return -1;
    }

    if (response->operation_code != STORAGE_OP_BLOCK_READ_RES)
    {
        log_error(logger, "Tipo de paquete inesperado para la respuesta de lectura de bloque (esperado=%u, recibido=%u)",
                  (unsigned)STORAGE_OP_BLOCK_READ_RES, (unsigned)response->operation_code);
        package_destroy(response);
        return -1;
    }

    uint32_t data_size;
    if (!package_read_uint32(response, &data_size))
    {
        log_error(logger, "Error al leer el tamaño de los datos del bloque");
        package_destroy(response);
        return -1;
    }

    size_t received_data_size;
    void *received_data = package_read_data(response, &received_data_size);
    if (!received_data || received_data_size != data_size)
    {
        log_error(logger, "Error al leer los datos del bloque o tamaño inconsistente");
        package_destroy(response);
        return -1;
    }

    *data = malloc(data_size);
    if (!*data)
    {
        log_error(logger, "Error al reservar memoria para los datos del bloque");
        package_destroy(response);
        return -1;
    }

    memcpy(*data, received_data, data_size);
    *size = data_size;
    package_destroy(response);

    log_debug(logger, "Lectura del bloque %u del archivo %s:%s realizada con éxito (%u bytes)",
              block_number, file, tag, data_size);

    return 0;
}

int create_file_in_storage(int storage_socket, int worker_id, char *file, char *tag)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_FILE_CREATE_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag))
    {
        return send_request_and_wait_ack(storage_socket, request,
                                         STORAGE_OP_FILE_CREATE_RES,
                                         "creación de archivo", worker_id);
    }

    log_error(logger, "Error al preparar el paquete para crear archivo");
    if (request)
        package_destroy(request);
    return -1;
}

int truncate_file_in_storage(int storage_socket, char *file, char *tag, size_t size, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_FILE_TRUNCATE_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag) &&
        package_add_uint32(request, (uint32_t)size))
    {

        int result = send_request_and_wait_ack(storage_socket, request,
                                               STORAGE_OP_FILE_TRUNCATE_RES,
                                               "truncate de archivo", worker_id);
        if (result == 0)
        {
            log_debug(logger, "Truncate del archivo %s:%s a %zu bytes realizado con éxito", file, tag, size);
        }
        return result;
    }

    log_error(logger, "Error al preparar el paquete para truncate");
    if (request)
        package_destroy(request);
    return -1;
}

int fork_file_in_storage(int storage_socket, char *file_src, char *tag_src, char *file_dst, char *tag_dst, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_TAG_CREATE_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file_src) &&
        package_add_string(request, tag_src) &&
        package_add_string(request, file_dst) &&
        package_add_string(request, tag_dst))
    {

        int result = send_request_and_wait_ack(storage_socket, request,
                                               STORAGE_OP_TAG_CREATE_RES,
                                               "fork (TAG) de archivo", worker_id);
        if (result == 0)
        {
            // El log info de la instrucción (pedido en el enunciado) lo debería hacer el intreter.
            log_debug(logger, "Fork del archivo %s:%s a %s:%s realizado con éxito", file_src, tag_src, file_dst, tag_dst);
        }
        return result;
    }

    log_error(logger, "Error al preparar el paquete para fork");
    if (request)
        package_destroy(request);
    return -1;
}

int commit_file_in_storage(int storage_socket, char *file, char *tag, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag))
    {

        int result = send_request_and_wait_ack(storage_socket, request,
                                               STORAGE_OP_TAG_COMMIT_RES,
                                               "commit de archivo", worker_id);
        if (result == 0)
        {
            // Idem log info de fork.
            log_debug(logger, "Commit del archivo %s:%s realizado con éxito", file, tag);
        }
        return result;
    }

    log_error(logger, "Error al preparar el paquete para commit");
    if (request)
        package_destroy(request);
    return -1;
}

int write_block_to_storage(int storage_socket, char *file, char *tag, uint32_t block_number, void *data, size_t size, int worker_id)
{
    t_log *logger = logger_get();
    t_package *request = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);

    if (request &&
        package_add_uint32(request, worker_id) &&
        package_add_string(request, file) &&
        package_add_string(request, tag) &&
        package_add_uint32(request, block_number) &&
        package_add_data(request, data, size))
    {

        int result = send_request_and_wait_ack(storage_socket, request,
                                               STORAGE_OP_BLOCK_WRITE_RES,
                                               "escritura de bloque", worker_id);
        if (result == 0)
        {
            log_debug(logger, "Escritura del bloque %u del archivo %s:%s realizada con éxito", block_number, file, tag);
        }
        return result;
    }

    log_error(logger, "Error al preparar el paquete para escritura de bloque");
    if (request)
        package_destroy(request);
    return -1;
}

int delete_file_in_storage(int storage_socket, char *file, char *tag, int worker_id)
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
