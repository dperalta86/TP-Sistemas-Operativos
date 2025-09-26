#ifndef STORAGE_SERVER_H_
#define STORAGE_SERVER_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include "connection/protocol.h"
#include "globals/globals.h"
#include "connection/serialization.h"
#include "operations/handshake.h"

int wait_for_client(int server_socket);
void* handle_client(void* arg);
void client_data_destroy(t_client_data *client_data);

#endif