#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <commons/log.h>
#include <config/worker_config.h>
#include <utils/logger.h>
#include <connections/master.h>
#include <connections/storage.h>
#include "worker_listener.h"
#include "query_executor.h"
#include "worker..h"
#include <strings.h>

static void validate_arguments(int argc, char *argv[], char **config_path, int *worker_id);
static t_worker_config *initialize_config(char *config_path);
static void initialize_logger(const t_worker_config *config);
static pt_replacement_t parse_replacement_algorithm(const char *algorithm);

int main(int argc, char *argv[])
{
    char *config_path;
    int worker_id;
    validate_arguments(argc, argv, &config_path, &worker_id);

    t_worker_config *config = initialize_config(config_path);
    initialize_logger(config);

    t_log *logger = logger_get();
    log_info(logger, "## Worker iniciado - ID=%d", worker_id);

    int socket_storage = -1;
    int socket_master = -1;
    memory_manager_t *mm = NULL;

    /* Handshake con Storage y Master */
    socket_storage = handshake_with_storage(config->storage_ip, config->storage_port, worker_id);
    if (socket_storage < 0)
        goto cleanup;

    socket_master = handshake_with_master(config->master_ip, config->master_port, worker_id);
    if (socket_master < 0)
        goto cleanup;

    /* Obtener tamaño de bloque */
    uint16_t block_size;
    if (get_block_size(socket_storage, &block_size, worker_id) < 0)
    {
        log_error(logger, "## Error al obtener tamaño de bloque del Storage");
        goto cleanup;
    }

    config->block_size = block_size;
    log_info(logger, "## Tamaño de bloque recibido: %d", config->block_size);

    pt_replacement_t replacement_algo = parse_replacement_algorithm(config->replacement_algorithm);
    mm = mm_create(
        config->memory_size,
        config->block_size,
        replacement_algo, config->memory_retardation);

    if (!mm)
    {
        log_error(logger, "## No se pudo inicializar la memoria interna");
        goto cleanup;
    }

    log_info(logger, "## Memoria interna creada - tamaño: %d - tamaño de pagina: %d - politica de reemplazo: %s",
             config->memory_size, config->block_size, config->replacement_algorithm);

    mm_set_storage_connection(mm, socket_storage, worker_id);

    /* Crear estado global */
    worker_state_t state = {
        .has_query = false,
        .should_stop = false,
        .ejection_requested = false,
        .is_executing = false,

        .master_socket = socket_master,
        .storage_socket = socket_storage,
        .config = config,
        .logger = logger,
        .memory_manager = mm,
        .worker_id = worker_id};

    pthread_mutex_init(&state.mux, NULL);
    pthread_cond_init(&state.new_query_cond, NULL);

    /* Crear hilos */
    pthread_t listener_tid, executor_tid;
    int rc_listener = pthread_create(&listener_tid, NULL, master_listener_thread, &state);
    if (rc_listener != 0)
    {
        log_error(logger, "## No se pudo crear el hilo master_listener_thread (error %d)", rc_listener);
        goto cleanup;
    }
    int rc_executor = pthread_create(&executor_tid, NULL, query_executor_thread, &state);
    if (rc_executor != 0)
    {
        log_error(logger, "## No se pudo crear el hilo query_executor_thread (error %d)", rc_executor);
        goto cleanup;
    }

    pthread_join(listener_tid, NULL);
    pthread_join(executor_tid, NULL);

cleanup:
    if (socket_master >= 0)
        close(socket_master);
    if (socket_storage >= 0)
        close(socket_storage);
    if (mm)
        mm_destroy(mm);
    if (config)
        destroy_worker_config(config);
    logger_destroy();
    return 0;
}

static void validate_arguments(int argc, char *argv[], char **config_path, int *worker_id)
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s [archivo_config] [ID Worker]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    *config_path = argv[1];
    *worker_id = atoi(argv[2]);

    if (*worker_id <= 0)
    {
        fprintf(stderr, "Error: ID Worker debe ser un número positivo\n");
        exit(EXIT_FAILURE);
    }
}

static t_worker_config *initialize_config(char *config_path)
{
    t_worker_config *config = create_worker_config(config_path);
    if (!config)
    {
        fprintf(stderr, "Error: No se pudo cargar el archivo de configuración '%s'\n", config_path);
        exit(EXIT_FAILURE);
    }
    return config;
}

static void initialize_logger(const t_worker_config *config)
{
    t_log_level log_level = log_level_from_string(config->log_level);
    if (logger_init("worker", log_level, true) < 0)
    {
        fprintf(stderr, "Error: No se pudo inicializar el sistema de logging\n");
        exit(EXIT_FAILURE);
    }
}

static pt_replacement_t parse_replacement_algorithm(const char *algorithm)
{
    if (strcasecmp(algorithm, "LRU") == 0)
    {
        return LRU;
    }
    else if (strcasecmp(algorithm, "CLOCK_M") == 0 || strcasecmp(algorithm, "CLOCK-M") == 0)
    {
        return CLOCK_M;
    }
    else
    {
        fprintf(stderr, "Error: Algoritmo de reemplazo desconocido '%s'. Usando LRU por defecto.\n", algorithm);
        return LRU;
    }
}
