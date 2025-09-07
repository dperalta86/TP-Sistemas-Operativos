#include <utils/hello.h>
#include <utils/server.h>

int main(int argc, char* argv[]) {
    saludar("query_control");

    // SOLO PARA TEST DE FUNCIÓN inicar_servidor hardcodeo ip y puerto
    int socket;
    socket = iniciar_servidor("127.0.0.1", "8001");
    printf("Socket %d creado con exito!", socket);
    return 0;
}
