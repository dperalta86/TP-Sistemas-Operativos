#include "cliente.h"

int conectar_servidor(char *ip, char *puerto)
{
    struct addrinfo hints, *server_info = NULL;
    int socket_cliente = -1;
    int resultado_obtener_server_info;
    int resultado_conexion;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Arma la información de la dirección del socket
    resultado_obtener_server_info = getaddrinfo(ip, puerto, &hints, &server_info);
    if (resultado_obtener_server_info != 0)
    {
        goto error;
    }

    // Crea el socket del cliente
    socket_cliente = socket(server_info->ai_family,
                            server_info->ai_socktype,
                            server_info->ai_protocol);
    if (socket_cliente == -1)
    {
        goto borrar_addrinfo;
    }

    // Intenta conectarse al servidor
    resultado_conexion = connect(socket_cliente,
                                 server_info->ai_addr,
                                 server_info->ai_addrlen);
    if (resultado_conexion == -1)
    {
        goto borrar_socket;
    }

    freeaddrinfo(server_info);
    return socket_cliente;

borrar_socket:
    close(socket_cliente);
borrar_addrinfo:
    freeaddrinfo(server_info);
error:
    return -1;
}


void liberar_conexion(int socket_cliente) {
    if (socket_cliente != -1) {
        close(socket_cliente);
    }
}
