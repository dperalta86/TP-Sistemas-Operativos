#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

/**
 * Permite conectarse a un servidor mediante sockets
 *
 * @param server_ip Dirección IP del servidor
 * @param server_port Puerto del servidor
 * @return El descriptor del socket del cliente o un -1 si hubo un error.
 */
int client_connect(const char *server_ip, const char *server_port);

/**
 * Cierra el socket del cliente
 *
 * @param client_socket Descriptor del socket del cliente
 */
void client_disconnect(const int client_socket);

#endif
