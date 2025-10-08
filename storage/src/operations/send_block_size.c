#include "send_block_size.h"

t_package* send_block_size(t_client_data *client_data) {
    t_package *response = package_create_empty(STORAGE_OP_WORKER_GET_BLOCK_SIZE_RES);
    if (!response) {
        log_error(g_storage_logger, "## Error al crear el paquete de respuesta para el tamaño del bloque");
        return NULL;
    }

    if(!package_add_uint16(response, g_storage_config->block_size)) {
        log_error(g_storage_logger, "## Envío de tamaño de bloque al Worker %s: no se pudo escribir en el buffer - Socket %d", client_data->client_id, client_data->client_socket);
        return NULL;
    }

    log_info(g_storage_logger, "## Enviando tamaño de bloque %d al Worker %s - Socket: %d", g_storage_config->block_size, client_data->client_id, client_data->client_socket);

    return response;
}