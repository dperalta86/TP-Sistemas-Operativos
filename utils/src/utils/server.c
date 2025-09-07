#include "server.h"

int iniciar_servidor(char *ip, char *puerto) {
	int socket_servidor;

	struct addrinfo hints, *servinfo;

    // Configuramos el socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = getaddrinfo(ip, puerto, &hints, &servinfo);
    if (rv != 0) {
        printf("getaddrinfo error: %s\n", gai_strerror(rv));
        freeaddrinfo(servinfo);
        return -1;
    }

	// Creamos el socket de escucha del servidor
        int serverSocket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (serverSocket == -1) {
        printf("Socket creation error\n%s", strerror(errno));
        freeaddrinfo(servinfo);
        return -1;
    }

	// Asociamos el socket a un puerto. Verifico si se asoció correctamente
    int yes = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        printf("setsockopt error\n%s", strerror(errno));
        freeaddrinfo(servinfo);
        return -1;
    }

    // Bindeo el puerto al socket
    if (bind(serverSocket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(serverSocket);
        printf("Bind error\n%s", strerror(errno));
        freeaddrinfo(servinfo);
        return -1;
    }

	// Escuchamos las conexiones entrantes
    if (listen(serverSocket, SOMAXCONN) == -1) {
        printf("Listen error\n%s", strerror(errno));
        freeaddrinfo(servinfo);
        return -1;
    }
    socket_servidor = serverSocket;
	freeaddrinfo(servinfo);

	return socket_servidor;
}