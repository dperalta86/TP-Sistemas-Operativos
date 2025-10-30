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
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <query_interpreter/query_interpreter.h>

#define MICROSECONDS_TO_SLEEP 1000000

typedef struct
{
    char query_path[PATH_MAX];
    int query_id;
    int program_counter;
    bool is_executing;
} query_context_t;

typedef struct
{
    pthread_mutex_t mux;
    bool has_query;
    bool should_stop;
    query_context_t current_query;
    int master_socket;
    int storage_socket;
    t_worker_config *config;
    t_log *logger;
    memory_manager_t *memory_manager;
    int worker_id;
} worker_state_t;

void *query_executor_thread(void *arg);
void *master_listener_thread(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Se deben ingresar los argumentos [archivo_config] y [ID Worker]\n");
        exit(EXIT_FAILURE);
    }

    char *config_path = argv[1];
    int worker_id = atoi(argv[2]);

    /* Cargar configuracion */
    t_worker_config *config = create_worker_config(config_path);
    if (!config)
    {
        fprintf(stderr, "Error al leer archivo de configuración\n");
        exit(EXIT_FAILURE);
    }

    /* Crear instancia de logger */
    t_log_level log_level = log_level_from_string(config->log_level);
    if (logger_init("worker", log_level, true) < 0)
    {
        fprintf(stderr, "Error al inicializar logger\n");
        destroy_worker_config(config);
        exit(EXIT_FAILURE);
    }

    t_log *logger = logger_get();
    log_info(logger, "## Worker iniciado - ID=%d", worker_id);

    /* Handshake con Storage y Master */
    int socket_storage = handshake_with_storage(config->storage_ip, config->storage_port, worker_id);
    if (socket_storage < 0)
        goto cleanup;

    int socket_master = handshake_with_master(config->master_ip, config->master_port, worker_id);
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

    memory_manager_t *mm = mm_create(
        config->memory_size,
        config->block_size,
        (pt_replacement_t)config->replacement_algorithm, config->memory_retardation);

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
        .current_query = {.is_executing = false},
        .master_socket = socket_master,
        .storage_socket = socket_storage,
        .config = config,
        .logger = logger,
        .memory_manager = mm,
        .worker_id = worker_id};
    pthread_mutex_init(&state.mux, NULL);

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
    if (config)
        destroy_worker_config(config);
    logger_destroy();
    mm_destroy(mm);
    pthread_mutex_destroy(&state.mux);
    return 0;
}

void *master_listener_thread(void *arg)
{
    worker_state_t *state = (worker_state_t *)arg;
    t_log *logger = state->logger;

    while (true)
    {
        t_package *pkg = package_receive(state->master_socket);
        if (!pkg)
        {
            log_error(logger, "## Se perdió la conexión con el Master");
            break;
        }

        switch (pkg->operation_code)
        {
        case OP_ASSIGN_QUERY:
        {
            uint32_t query_id, pc;
            char *path = NULL;

            if (!package_read_uint32(pkg, &query_id))
                break;
            if (!package_read_uint32(pkg, &pc))
                break;
            path = package_read_string(pkg);
            if (!path)
                break;

            pthread_mutex_lock(&state->mux);
            strcpy(state->current_query.query_path, path);
            state->current_query.program_counter = pc;
            state->current_query.query_id = query_id;
            state->has_query = true;
            state->should_stop = false;
            
            mm_set_query_id(state->memory_manager, query_id);
            
            pthread_mutex_unlock(&state->mux);

            log_info(logger, "## Se asignó Query %d - Program Counter=%d - Ruta=%s", query_id, pc, path);
            free(path);
            break;
        }

        case OP_EJECT_QUERY:
        {
            uint32_t query_id;
            if (!package_read_uint32(pkg, &query_id))
                break;

            pthread_mutex_lock(&state->mux);
            if (state->has_query && state->current_query.query_id == query_id)
                state->should_stop = true;
            pthread_mutex_unlock(&state->mux);

            log_info(logger, "## Se recibió solicitud de desalojo para Query %d", query_id);
            break;
        }

        case OP_END_QUERY:
            log_info(logger, "## Se recibió solicitud de finalización del Worker");
            pthread_mutex_lock(&state->mux);
            state->should_stop = true;
            state->has_query = false;
            pthread_mutex_unlock(&state->mux);
            package_destroy(pkg);
            return NULL;

        default:
            log_warning(logger, "## Código de operación desconocido: %d", pkg->operation_code);
            break;
        }

        package_destroy(pkg);
    }

    return NULL;
}

void *query_executor_thread(void *arg)
{
    worker_state_t *state = (worker_state_t *)arg;

    while (true)
    {
        pthread_mutex_lock(&state->mux);

        if (state->should_stop && !state->has_query)
        {
            pthread_mutex_unlock(&state->mux);
            break;
        }

        if (!state->has_query || state->should_stop)
        {
            pthread_mutex_unlock(&state->mux);
            usleep(MICROSECONDS_TO_SLEEP);
            continue;
        }

        query_context_t ctx = state->current_query;
        state->current_query.is_executing = true;
        pthread_mutex_unlock(&state->mux);

        char *raw_instruction = NULL;
        if (fetch_instruction(ctx.query_path, ctx.program_counter, &raw_instruction) < 0)
        {
            log_error(state->logger, "## Query %d: Error en FETCH - Program Counter: %d", ctx.query_id, ctx.program_counter);
            goto finish_query;
        }

        char **tokens = string_split(raw_instruction, " ");
        char *op_instruction = tokens[0];

        log_info(state->logger, "## Query %d: FETCH - Program Counter: %d - %s", ctx.query_id, ctx.program_counter, op_instruction);

        instruction_t *instruction = malloc(sizeof(instruction_t));
        if (decode_instruction(raw_instruction, instruction) < 0)
        {
            log_error(state->logger, "## Query %d: Error al decodificar - %s", ctx.query_id, op_instruction);
            free(raw_instruction);
            free(instruction);
            goto finish_query;
        }

        int exec_res = execute_instruction(
            instruction,
            state->storage_socket,
            state->master_socket,
            state->memory_manager,
            ctx.query_id,
            state->worker_id);

        bool end_detected = (instruction->operation == END);

        free_instruction(instruction);
        free(instruction);

        if (exec_res < 0)
        {
            log_error(state->logger, "## Query %d: Falló la instrucción - %s", ctx.query_id, op_instruction);
            string_array_destroy(tokens);
            free(raw_instruction);
            goto finish_query;
        }
        log_info(state->logger, "## Query %d: Instrucción realizada: %s", ctx.query_id, raw_instruction);
        string_array_destroy(tokens);
        free(raw_instruction);

        pthread_mutex_lock(&state->mux);
        if (!end_detected)
            state->current_query.program_counter++;
        bool eject = state->should_stop && !end_detected;
        pthread_mutex_unlock(&state->mux);

        if (eject)
        {
            log_info(state->logger, "## Query %d: Desalojada por pedido del Master", ctx.query_id);
            pthread_mutex_lock(&state->mux);
            state->has_query = false;
            state->should_stop = false;
            state->current_query.is_executing = false;
            pthread_mutex_unlock(&state->mux);
            continue;
        }

        if (end_detected)
        {
            log_info(state->logger, "## Query %d: Finalizada", ctx.query_id);
            pthread_mutex_lock(&state->mux);
            state->has_query = false;
            state->current_query.is_executing = false;
            pthread_mutex_unlock(&state->mux);
            continue;
        }

        usleep(state->config->memory_retardation * 1000);
        continue;

    finish_query:
        pthread_mutex_lock(&state->mux);
        state->has_query = false;
        state->current_query.is_executing = false;
        pthread_mutex_unlock(&state->mux);
        log_info(state->logger, "## Query %d: Abortada", ctx.query_id);
    }

    return NULL;
}
