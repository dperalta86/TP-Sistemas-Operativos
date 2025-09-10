#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utils/server.h>
#include <utils/client_socket.h>
#include <config/storage_config.h>
#include <logger/logger.h>

int start_storage_server();

t_storage_config* g_storage_config;
t_log* g_storage_logger;

int main(int argc, char* argv[]) {
    char* config_file_path[50];

    if (argc == 1) {
        strcpy(config_file_path, "storage.config");
    } else if (argc == 2) {
        strcpy(config_file_path, argv[1]);
    } else {
        fprintf(stderr, "Solo se puede ingresar el argumento [archivo_config]");
        goto error;
    }

    g_storage_config = create_storage_config(config_file_path);
    if (storage_config == NULL) {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        goto error;
    }

    if (logger_init("storage", storage_config->log_level, true) != 0) {
        fprintf(stderr, "No se pudo inicializar el logger global\n");
        goto clean;
    }

    g_storage_logger = logger_get();

    int socket = start_server(g_storage_config->storage_ip, g_storage_config->storage_port);

    destroy_storage_config(storage_config);
    logger_destroy();
    exit(EXIT_SUCCESS);

clean:
    destroy_storage_config(storage_config);
    logger_destroy();
error:
    exit(EXIT_FAILURE);
}