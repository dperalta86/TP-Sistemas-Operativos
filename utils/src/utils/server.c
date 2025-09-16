#include "server.h"

int start_server(char *ip, char *port) {

    if (ip == NULL || port == NULL) 
    {
        goto error;
    }

    struct addrinfo hints, *server_info;

    // Configuramos el socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int result = getaddrinfo(ip, port, &hints, &server_info);
    if (result != 0) {
        goto error;
    }

    // Creamos el socket de escucha del servidor
    int server_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_socket == -1) {
        goto clean;
    }

    // Asociamos el socket a un puerto. Verifico si se asoció correctamente
    int yes = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        goto clean;
    }

    // Bindeo el puerto al socket
    if (bind(server_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        goto bind_error;
    }

    // Escuchamos las conexiones entrantes
    if (listen(server_socket, SOMAXCONN) == -1) {
        goto clean;
    }
    freeaddrinfo(server_info);

    return server_socket;


clean:
    freeaddrinfo(server_info);
bind_error:
    close(server_socket);
    freeaddrinfo(server_info);
error:
    return -1;
}

int connect_to_server(char *ip, char *port) {
    struct addrinfo hints, *server_info;
    int retval;

    if (ip == NULL || port == NULL) {
        retval = -1;
        goto error;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(ip, port, &hints, &server_info);
    if (result != 0) {
        retval = -2;
        goto error;
    }

    int client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (client_socket == -1) {
        retval = -3;
        goto clean;
    }

    if (connect(client_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        retval = -4;
        goto connect_error;
    }

    freeaddrinfo(server_info);
    return client_socket;

connect_error:
    close(client_socket);
clean:
    freeaddrinfo(server_info);
error:
    return retval;
}
