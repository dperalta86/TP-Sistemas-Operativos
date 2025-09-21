#include "server.h"

int wait_for_client(int server_socket)
{
	struct sockaddr_in client_address;
    socklen_t address_size = sizeof(struct sockaddr_in);

	// Aceptamos un nuevo cliente
    int client_socket = accept(server_socket, (void*)&client_address, &address_size);
    if (client_socket == -1) {
        log_error(g_storage_logger, "Error al aceptar un Worker.");
        return -1;
    }

	return client_socket;
}

void* handle_client(void* arg) {
    t_client_data* client_data = (t_client_data*)arg;
    int client_socket = client_data->client_socket;

    while (1)
    {
        t_package *request = package_receive(client_socket);
        if (!request) {
            log_error(g_storage_logger, "Error en la recepción de la solicitud del Worker");
            goto cleanup;
        }
        
        t_package *response;
        switch (request->operation_code) {
            case STORAGE_OP_WORKER_SEND_ID_REQ:
                response = handle_handshake(request, client_data);
    
                break;
            default:
                log_error(g_storage_logger, "Código de operación desconocido recibido del Worker: %u", request->operation_code);
                package_destroy(request);
                goto cleanup;
        }

        if (!response)
        {
            goto cleanup;
        }
    
        package_send(response, client_socket);
        
        package_destroy(response);
        package_destroy(request);
    }
    
cleanup:
    close(client_socket);
    free(client_data);
    return 0;
}

void client_data_destroy(t_client_data *client_data) {
    free(client_data->client_id);
    free(client_data);
}

