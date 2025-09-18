#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <config/worker_config.h>
#include <utils/logger.h>
#include <connections/master.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Se deben ingresar los argumentos [archivo_config] y [ID Worker]");
        goto error;
    }

    char *config_file_path = argv[1];
    t_worker_config *worker_config = create_worker_config(config_file_path);
    if (worker_config == NULL)
    {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        goto error;
    }

    t_log_level log_level = log_level_from_string(worker_config->log_level);

    if (logger_init("worker", log_level, true) < 0)
    {
        fprintf(stderr, "No se pudo inicializar el logger global\n");
        goto clean;
    }
    t_log *logger = logger_get();
    log_info(logger, "## Logger inicializado");

    int client_socket_master = handshake_with_master(worker_config->master_ip, worker_config->master_port);

    // int client_socket_storage = client_connect(worker_config->storage_ip, worker_config->storage_port);
    // if (client_socket_storage < 0)
    // {
    //     log_error(logger, "## No se pudo establecer conexión con Master. IP=%s:%s", worker_config->storage_ip, worker_config->storage_port);
    //     goto clean;
    // }

    // log_info(logger, "## Se establecio conexión con Storage. IP=%s:%s", worker_config->storage_ip, worker_config->storage_port);

    destroy_worker_config(worker_config);
    logger_destroy();
    close(client_socket_master);
    // close(client_socket_storage);
    exit(EXIT_SUCCESS);

clean:
    destroy_worker_config(worker_config);
    logger_destroy();
error:
    exit(EXIT_FAILURE);
}
