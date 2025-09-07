#ifndef CLIENTE_H
#define CLIENTE_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>

/**
 * Permite conectarse a un servidor mediante sockets
 * 
 * @param ip Dirección IP del servidor
 * @param puerto Puerto del servidor
 * @return El socket del cliente
 */
int conectar_servidor(char *ip, char *puerto);

#endif
