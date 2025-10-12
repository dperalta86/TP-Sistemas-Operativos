#include <utils/server.h>
#include <utils/utils.h>
#include <commons/config.h>
#include <commons/log.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include <init_master.h>
#include <connection/protocol.h>
#include <connection/serialization.h>
#include <query_control_manager.h>
#include <worker_manager.h>
#include <config/master_config.h>

#define MODULO "MASTER"
#define LOG_LEVEL LOG_LEVEL_DEBUG //inicialmente DEBUG, luego se setea desde el config

void* handle_client(void* arg);

typedef struct {
    int client_socket;
    t_master *master;
} t_client_data;

int main(int argc, char* argv[]) {
    // Verifico que se hayan pasado los parametros correctamente
    if (argc != 2) 
    {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion\nUso: %s [archivo_config]\n", argv[0]);
        goto error;
    }

    // Obtengo el directorio actual
    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) 
    {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        goto error;
    }
    
    // Inicializo el logger
    t_log_level log_level = LOG_LEVEL;
    t_log* logger = create_logger(current_directory, MODULO, true, log_level);
    if (logger == NULL) 
    {
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

    // Seteo el nivel de logeo desde el config
    logger->detail = master_config->log_level;

    log_debug(logger, "Configuracion leida: \n\tIP_ESCUCHA=%s\n\tPUERTO_ESCUCHA=%s\n\tALGORITMO_PLANIFICACION=%s\n\tTIEMPO_AGING=%d\n\tLOG_LEVEL=%s",
             master_config->ip, master_config->port, master_config->scheduler_algorithm, master_config->aging_time, log_level_as_string(master_config->log_level));

    // Inicializo la estructura principal del Master (tablas, datos de config, hilos, etc.)
    t_master *master = init_master(master_config->ip, master_config->port, master_config->aging_time, master_config->scheduler_algorithm, logger);
    
    // Destruyo master_config
    destroy_master_config_instance(master_config);
    master_config = NULL;
    
    // Inicio el servidor
    int server_socket_fd = start_server(master->ip, master->port);

    if (server_socket_fd < 0) 
    {
        log_error(logger, "Error al iniciar el servidor en %s:%s", master->ip, master->port);
        goto clean;
    }

    log_info(logger, "Socket %d creado con exito!", server_socket_fd);

    // Bucle principal para aceptar conexiones entrantes
    while (1) {
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket_fd = accept(server_socket_fd, &client_addr, &client_addr_len);
        if (client_socket_fd < 0) 
        {
            log_error(logger, "Error al aceptar conexion del cliente");
            continue;
        }

        log_info(logger, "Cliente conectado en socket %d", client_socket_fd);

        t_client_data *client_data = malloc(sizeof(t_client_data));
        if (client_data == NULL) {
            log_error(logger, "Error al asignar memoria para datos del cliente");
            close(client_socket_fd);
            continue;
        }  
        client_data->client_socket = client_socket_fd;
        client_data->master = master;


        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, client_data) != 0) {
            log_error(logger, "Error al crear hilo para cliente %d", client_socket_fd);
            close(client_socket_fd);
            free(client_data);
            continue;
        }

        pthread_detach(client_thread);
    }

clean:
    if (master) destroy_master(master);
    if (master_config) destroy_master_config_instance(master_config);
    if (logger) log_destroy(logger);
    return 1;
error:
    return -1;

    return 0;
}

void* handle_client(void* arg) {
    t_client_data *client_data = (t_client_data*)arg;
    int client_socket = client_data->client_socket;
    t_master *master = client_data->master;

    while (1) {
        t_package *required_package = package_receive(client_socket);

        if (required_package == NULL) {
            log_error(master->logger, "Error al recibir el paquete del cliente %d, se cierra conexión y libera socket.", client_socket);
            break; // Sale del bucle y cierra la conexión
        }

        switch (required_package->operation_code)
        {
            case OP_QUERY_HANDSHAKE:
                log_debug(master->logger, "Recibido OP_QUERY_HANDSHAKE de socket %d", client_socket);
                if (manage_query_handshake(client_socket, master->logger) == 0) {
                    log_info(master->logger, "Handshake completado con Query Control en socket %d", client_socket);
                }       
                break;
            case OP_QUERY_FILE_PATH:
                log_debug(master->logger, "Recibido OP_QUERY_FILE_PATH de socket %d", client_socket);
                if (manage_query_file_path(required_package, client_socket, master) != 0) {
                    log_error(master->logger, "Error al manejar OP_QUERY_FILE_PATH del cliente %d", client_socket);
                }
                break;
            
            // Worker
            case OP_WORKER_HANDSHAKE_REQ:
                log_debug(master->logger, "Recibido OP_WORKER_HANDSHAKE de socket %d", client_socket);
                if (manage_worker_handshake(required_package->buffer, client_socket, master) == 0) {
                    log_info(master->logger, "Handshake completado con worker en socket %d", client_socket);
                }              
                break;
            case OP_WORKER_READ_MESSAGE_REQ:
                log_debug(master->logger, "Recibido OP_WORKER_READ_MESSAGE de socket %d", client_socket);
                if (manage_read_message_from_worker(required_package->buffer, client_socket, master) != 0) {
                    log_error(master->logger, "Error al manejar OP_WORKER_READ_MESSAGE del cliente %d", client_socket);
                }
                break;
            default:
                log_warning(master->logger, "Operacion desconocida recibida del cliente %d", client_socket);
                break;
        }
        if(required_package)
        {
            package_destroy(required_package); // Libera el paquete recibido
        }
            
    }

    close(client_socket);
    if(client_data)
    {
        free(client_data);
    }
        
    return NULL;
}