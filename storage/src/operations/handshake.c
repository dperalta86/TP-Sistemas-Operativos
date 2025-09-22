#include "operations/handshake.h"

t_package* handle_handshake(t_package *package, t_client_data *client_data) {

    // Contabiliza el worker que se conecta
    pthread_mutex_lock(&g_worker_counter_mutex);
    g_worker_counter++;
    pthread_mutex_unlock(&g_worker_counter_mutex);

    // Obtiene el worker id del package recibido
    t_buffer *received_buffer = package->buffer;
    char *worker_id = buffer_read_string(received_buffer);
    if (!worker_id)
    {
        log_error(g_storage_logger, "## Handshake de Worker: no se pudo obtener el buffer del paquete recibido - Socket: %d", client_data->client_socket);
        
        return NULL;
    }
    
    client_data->client_id = worker_id;

    log_info(g_storage_logger, "## Se conecta el Worker %s - Cantidad de Workers: %d", client_data->client_id, g_worker_counter);    
    log_info(g_storage_logger, "## Handshake recibido del Worker %s - Socket: %d", client_data->client_id, client_data->client_socket);

    // Prepara la respuesta
    t_buffer *buffer = buffer_create(sizeof(uint8_t));
    if (!buffer)
    {
        log_error(g_storage_logger, "## Handshake del Worker %s: no se pudo crear el buffer para respuesta - Socket: %d", client_data->client_id, client_data->client_socket);

        return NULL;
    }
    buffer_write_uint8(buffer, STORAGE_OP_WORKER_SEND_ID_RES);

    t_package *response = package_create(STORAGE_OP_WORKER_SEND_ID_RES, buffer);    
    if (!response)
    {
        log_error(g_storage_logger, "## Handshake del Worker %s: no se pudo crear el paquete para respuesta - Socket: %d", client_data->client_id, client_data->client_socket);
        buffer_destroy(buffer);

        return NULL;
    }
    
    return response;
}