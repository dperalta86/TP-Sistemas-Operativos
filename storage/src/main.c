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
#include <pthread.h>
#include "globals/globals.h"
#include "server/server.h"
#include "helper/helper.h"

#define MODULE "STORAGE"
#define DEFAULT_CONFIG_PATH "./src/storage.config"

t_storage_config* g_storage_config;
t_log* g_storage_logger;

int main(int argc, char* argv[]) {
    // Obtiene posibles parametros de entrada
    char config_file_path[PATH_MAX];

    if (argc == 1) {
        strlcpy(config_file_path, DEFAULT_CONFIG_PATH, PATH_MAX);
    } else if (argc == 2) {
        strlcpy(config_file_path, argv[1], PATH_MAX);
    } else {
        fprintf(stderr, "Solo se puede ingresar el argumento [archivo_config]\n");
        goto error;
    }

    // Crea storage config como variable global
    g_storage_config = create_storage_config(config_file_path);
    if (g_storage_config == NULL) {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        goto error;
    }

    // Crea storage logger como variable global
    t_log_level log_level = log_level_from_string(g_storage_config->log_level);

    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        goto clean;
    }

    g_storage_logger = create_logger(current_directory, MODULE, false, log_level);
    if (g_storage_logger == NULL) {
        fprintf(stderr, "No se pudo crear el logger\n");
        goto clean;
    }

    log_debug(g_storage_logger, "Logger creado con nivel %s", g_storage_config->log_level);

    // Inicia servidor
    int socket = start_server(g_storage_config->storage_ip, g_storage_config->storage_port);
    if (socket == -1) {
        log_error(g_storage_logger, "No se pudo iniciar el servidor en %s:%s\n", g_storage_config->storage_ip, g_storage_config->storage_port);
        goto clean;
    }

    log_info(g_storage_logger, "Servidor iniciado en %s:%s", g_storage_config->storage_ip, g_storage_config->storage_port);

	while (1) {
        int client_fd = wait_for_client(socket);
        if (client_fd == -1) {
            goto clean;
        }
        
        t_client_data* client_data = malloc(sizeof(t_client_data));
        client_data->client_socket = client_fd;
        //client_data->logger = logger;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, client_data) != 0) {
            log_error(g_storage_logger, "Error al crear hilo para cliente %d", client_fd);
            close(client_fd);
            free(client_data);
            continue;
        }

        pthread_detach(client_thread);
	}

    destroy_storage_config(g_storage_config);
    log_destroy(g_storage_logger);
    exit(EXIT_SUCCESS);

clean:
    destroy_storage_config(g_storage_config);
    log_destroy(g_storage_logger);
error:
    exit(EXIT_FAILURE);
}