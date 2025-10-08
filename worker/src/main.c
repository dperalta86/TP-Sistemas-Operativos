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
#include <connections/storage.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Se deben ingresar los argumentos [archivo_config] y [ID Worker]\n");
        goto error;
    }

    char *config_file_path = argv[1];
    char *worker_id = argv[2];

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

    int client_socket_master =
        handshake_with_master(worker_config->master_ip,
                              worker_config->master_port,
                              worker_id);
    if (client_socket_master < 0)
    {
        goto clean;
    }

    int client_socket_storage =
        handshake_with_storage(worker_config->storage_ip,
                               worker_config->storage_port,
                               worker_id);
    if (client_socket_storage < 0)
    {
        goto clean;
    }

    uint16_t block_size;
    if(get_block_size(client_socket_storage, &block_size)) 
    {
       goto clean; 
    }
    worker_config->block_size = (int)block_size;
    log_info(logger, "## Tamaño de bloque recibido desde Storage: %d", worker_config->block_size);

    destroy_worker_config(worker_config);
    logger_destroy();
    if (client_socket_master >= 0)
        close(client_socket_master);
    if (client_socket_storage >= 0)
        close(client_socket_storage);
    exit(EXIT_SUCCESS);

clean:
    destroy_worker_config(worker_config);
    logger_destroy();
error:
    exit(EXIT_FAILURE);
}
