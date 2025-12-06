#include "operations/handshake.h"

t_package* handle_handshake(t_package *package, t_client_data *client_data) {

    // Contabiliza el worker que se conecta
    pthread_mutex_lock(&g_worker_counter_mutex);
    g_worker_counter++;
    pthread_mutex_unlock(&g_worker_counter_mutex);

    // Obtiene el worker id del package recibido
    uint32_t worker_id;
    if(!package_read_uint32(package, &worker_id))
    {
        log_error(g_storage_logger, "## Handshake de Worker: no se pudo obtener el worker id - Socket: %d", client_data->client_socket);
        
        return NULL;
    }
    
    client_data->client_id = string_itoa((int)worker_id);

    log_info(g_storage_logger, "## Se conecta el Worker %s - Cantidad de Workers: %d", client_data->client_id, g_worker_counter);    
    log_info(g_storage_logger, "## Handshake recibido del Worker %s - Socket: %d", client_data->client_id, client_data->client_socket);

    // Prepara la respuesta
    t_package *response = package_create_empty(STORAGE_OP_WORKER_SEND_ID_RES);
    if (!response)
    {
        log_error(g_storage_logger, "## Handshake del Worker %s: no se pudo crear el paquete de respuesta - Socket: %d", client_data->client_id, client_data->client_socket);

        return NULL;
    }
    
    return response;
}