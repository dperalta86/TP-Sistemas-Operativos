#include <utils/server.h>
#include <utils/utils.h>
#include <config/master_config.h>
#include <commons/config.h>
#include <commons/log.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define MODULO "MASTER"
#define LOG_LEVEL "DEBUG"

void* handle_client(void* arg);

typedef struct {
    int client_socket;
    t_log* logger;
} t_client_data;

int main(int argc, char* argv[]) {
    // Verifico que se hayan pasado los parametros correctamente
    if (argc != 2) {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion\nUso: %s [archivo_config]\n", argv[0]);
        goto error;
    }

    // Obtengo el directorio actual
    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        goto error;
    }
    
    // Inicializo el logger
    t_log* logger = create_logger(current_directory, MODULO, true, LOG_LEVEL);
    if (logger == NULL) {
        fprintf(stderr, "Error al crear el logger\n");
        goto error;
    }

    char* path_config_file = argv[1];

    // Cargo el archivo de configuracion 
    t_master_config *master_config = create_master_config(path_config_file);
    if (!master_config)
    {
        log_error(logger, "Error al leer el archivo de configuracion %s\n", path_config_file);
        goto clean;
    }

    log_debug(logger, "Configuracion leida: \n\tIP_ESCUCHA=%s\n\tPUERTO_ESCUCHA=%s\n\tALGORITMO_PLANIFICACION=%s\n\tTIEMPO_AGING=%d\n\tLOG_LEVEL=%s",
             master_config->ip, master_config->port, master_config->scheduler_algorithm, master_config->aging_time, LOG_LEVEL);

    // Inicio el servidor
    int socket = start_server(master_config->ip, master_config->port);

    if (socket == -1) {
        log_error(logger, "Error al iniciar el servidor en %s:%s", master_config->ip, master_config->port);
        goto clean;
    }

    log_info(logger, "Socket %d creado con exito!", socket);

    while (1) {
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(socket, &client_addr, &client_addr_len);
        if (client_socket == -1) {
            log_error(logger, "Error al aceptar conexion del cliente");
            continue;
        }

        log_info(logger, "Cliente conectado en socket %d", client_socket);

        t_client_data* client_data = malloc(sizeof(t_client_data));
        client_data->client_socket = client_socket;
        client_data->logger = logger;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, client_data) != 0) {
            log_error(logger, "Error al crear hilo para cliente %d", client_socket);
            close(client_socket);
            free(client_data);
            continue;
        }

        pthread_detach(client_thread);
    }

clean:
    destroy_config(master_config);
    log_destroy(logger);
    return 1;
error:
    return -1;

    return 0;
}

void* handle_client(void* arg) {
    t_client_data* client_data = (t_client_data*)arg;
    int client_socket = client_data->client_socket;
    t_log* logger = client_data->logger;

    char buffer[256];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        log_error(logger, "Error al recibir mensaje del cliente %d", client_socket);
        goto cleanup;
    }

    buffer[bytes_received] = '\0';

    // Verificar si es un handshake de Query Control
    if (strcmp(buffer, "QUERY_CONTROL_HANDSHAKE") == 0) {
        log_info(logger, "Se conecto un Query Control - Socket: %d", client_socket);

        // Enviar respuesta de handshake
        char* response = "HANDSHAKE_OK";
        if (send(client_socket, response, strlen(response), 0) == -1) {
            log_error(logger, "Error al enviar respuesta de handshake al Query Control %d", client_socket);
        } else {
            log_info(logger, "Handshake completado con Query Control en socket %d", client_socket);
        }
    } else {
        log_warning(logger, "Mensaje desconocido del cliente %d: %s", client_socket, buffer);
    }

cleanup:
    close(client_socket);
    free(client_data);
    return NULL;
}
