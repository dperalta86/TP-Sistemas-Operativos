#include "client_socket.h"

int client_connect(const char *server_ip, const char *server_port)
{
    if (server_ip == NULL || server_port == NULL) {
        goto error;
    }

    struct addrinfo hints, *server_info = NULL;
    int client_socket = -1;
    int getaddrinfo_result;
    int connection_result;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Arma la información de la dirección del socket
    getaddrinfo_result = getaddrinfo(
        server_ip,
        server_port,
        &hints,
        &server_info);

    if (getaddrinfo_result != 0)
    {
        goto error;
    }

    // Crea el socket del cliente
    client_socket = socket(server_info->ai_family,
                           server_info->ai_socktype,
                           server_info->ai_protocol);
    if (client_socket == -1)
    {
        goto clean_addrinfo;
    }

    // Intenta conectarse al servidor
    connection_result = connect(client_socket,
                               server_info->ai_addr,
                               server_info->ai_addrlen);
    if (connection_result == -1)
    {
        goto clean_socket;
    }

    freeaddrinfo(server_info);
    return client_socket;

clean_socket:
    close(client_socket);
clean_addrinfo:
    if (server_info != NULL) freeaddrinfo(server_info);
error:
    return -1;
}

void client_disconnect(int client_socket)
{
    if (client_socket != -1)
    {
        close(client_socket);
    }
}
