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
#include <utils/utils.h>
#include <string.h>

t_storage_config* g_storage_config;
t_log* g_storage_logger;

int main(int argc, char* argv[]) {
    char config_file_path[PATH_MAX];

    if (argc == 1) {
        strncpy(config_file_path, "./src/storage.config", PATH_MAX - 1);
        config_file_path[PATH_MAX - 1] = '\0';
    } else if (argc == 2) {
        strncpy(config_file_path, argv[1], PATH_MAX - 1);
        config_file_path[PATH_MAX - 1] = '\0';
    } else {
        fprintf(stderr, "Solo se puede ingresar el argumento [archivo_config]\n");
        goto error;
    }

    g_storage_config = create_storage_config(config_file_path);
    if (g_storage_config == NULL) {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        goto error;
    }

    g_storage_logger = create_logger("./", "storage", false, g_storage_config->log_level);

    int socket = start_server(g_storage_config->storage_ip, g_storage_config->storage_port);
    if (socket == -1) {
        fprintf(stderr, "No se pudo iniciar el servidor en %s:%s\n", g_storage_config->storage_ip, g_storage_config->storage_port);
        goto error;
    }
    log_info(g_storage_logger, "Servidor iniciado en %s:%s", g_storage_config->storage_ip, g_storage_config->storage_port);

    destroy_storage_config(g_storage_config);
    //logger_destroy(); TODO: Agregar cuando se haga refactor de logger
    exit(EXIT_SUCCESS);

clean:
    destroy_storage_config(g_storage_config);
    //logger_destroy(); TODO: Agregar cuando se haga refactor de logger
error:
    exit(EXIT_FAILURE);
}