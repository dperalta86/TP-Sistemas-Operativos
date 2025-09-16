#ifndef SERVER_H
#define SERVER_H

// Bibliotecas estandar
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

// Prototipos

/**
 * @brief Inicia un servidor en la ip y puerto correspondientes
 * 
 * @param ip: ip del servidor que estamos creando
 * @param puerto: puerto del servidor que estamos creando
 * @return int: ID del file descriptor del socket del servidor
 * 
 * @example int socket = start_server("192.168.0.1", "2340");
 */
int start_server(char *ip, char *puerto);

/**
 * @brief Inicia un cliente conectandose a un servidor en la ip y puerto correspondientes
 * 
 * @param ip: ip del servidor al que nos conectamos
 * @param puerto: puerto del servidor al cual nos conectamos
 * @return int: ID del file descriptor del socket creado con el servidor
 * 
 * @example int socket = connect_to_server("192.168.0.1", "2340");
 */
int connect_to_server(char *ip, char *puerto);

#endif