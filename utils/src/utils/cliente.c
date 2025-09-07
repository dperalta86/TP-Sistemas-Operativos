#include "cliente.h"

int conectar_servidor(char *ip, char *puerto)
{
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Arma la información de la dirección del socket
    int resultado_obtener_server_info = getaddrinfo(
        ip,
        puerto,
        &hints,
        &server_info);

    if (resultado_obtener_server_info != 0)
    {
        return -1;
    }

    // Crea el socket del cliente
    int socket_cliente = socket(
        server_info->ai_family,
        server_info->ai_socktype,
        server_info->ai_protocol);

    if (socket_cliente == -1)
    {
        freeaddrinfo(server_info);
        return -1;
    }

    // Intenta conectarse al servidor
    int resultado_conexion = connect(
        socket_cliente,
        server_info->ai_addr,
        server_info->ai_addrlen);

    if (resultado_conexion == -1)
    {
        freeaddrinfo(server_info);
        return -1;
    }

    // Libera la memoria del puntero con la info. de dirección del servidor
    freeaddrinfo(server_info);

    return socket_cliente;
}

void liberar_conexion(int socket_cliente) {
    if (socket_cliente != -1) {
        close(socket_cliente);
    }
}
