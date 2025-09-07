#ifndef CLIENTE_H_UTILS
#define CLIENTE_H_UTILS

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
 * @param ip Dirección IP del servidor
 * @param puerto Puerto del servidor
 * @return El descriptor del socket del cliente o un -1 si hubo un error.
 */
int conectar_servidor(char *ip, char *puerto);

/**
 * Libera la conexión del servidor
 * 
 * @param socket_cliente Descriptor del socket del cliente
 */
void liberar_conexion(int socket_cliente);

#endif
