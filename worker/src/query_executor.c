#include "query_executor.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <utils/logger.h>
#include <query_interpreter/query_interpreter.h>

static bool fetch_next_query(worker_state_t *state);
static query_result_t execute_single_instruction(worker_state_t *state, query_context_t *ctx, int *next_pc);
static void notify_master_query_error(worker_state_t *state, int query_id, int pc);

void *query_executor_thread(void *arg)
{
    worker_state_t *state = (worker_state_t *)arg;

    while (true)
    {
        if (!fetch_next_query(state))
            break;

        query_result_t result = QUERY_RESULT_OK;
        query_context_t ctx;
        int next_pc;

        pthread_mutex_lock(&state->mux);
        ctx = state->current_query;
        state->ejection_requested = false;
        pthread_mutex_unlock(&state->mux);

        while (result == QUERY_RESULT_OK)
        {
            next_pc = ctx.program_counter;
            result = execute_single_instruction(state, &ctx, &next_pc);

            pthread_mutex_lock(&state->mux);
            if (result == QUERY_RESULT_OK)
            {
                state->current_query.program_counter = next_pc;
                ctx.program_counter = next_pc;
            }
            pthread_mutex_unlock(&state->mux);
        }

        pthread_mutex_lock(&state->mux);

        if (result == QUERY_RESULT_EJECT)
        {
            state->has_query = false;
            state->is_executing = false;
        }
        else
        {
            if (result == QUERY_RESULT_ERROR)
            {
                mm_flush_all_dirty(state->memory_manager);
                notify_master_query_error(state, ctx.query_id, ctx.program_counter);
            }

            /*
             * Sólo limpiar la bandera has_query si aún corresponde a la query que
             * nosotros estábamos ejecutando. Es posible que durante la ventana entre
             * el fin de ejecución y este bloqueo, el hilo listener ya haya asignado
             * una nueva query (nuevo query_id). En ese caso no debemos borrar
             * has_query porque eso cancelaría la asignación nueva y dejaría el
             * worker colgado.
             */
            if (state->has_query == true) {
                if (state->current_query.query_id == ctx.query_id) {
                    state->has_query = false;
                } else {
                    /* Otra query ya fue asignada; conservar has_query true */
                }
            }

            state->is_executing = false;
            log_info(state->logger, "## Query %d: %s",
                     ctx.query_id,
                     (result == QUERY_RESULT_END ? "Finalizada" : "Abortada"));
        }

        pthread_mutex_unlock(&state->mux);
    }

    return NULL;
}

static bool fetch_next_query(worker_state_t *state)
{
    pthread_mutex_lock(&state->mux);

    while (!state->has_query && !state->should_stop)
    {
        pthread_cond_wait(&state->new_query_cond, &state->mux);
    }

    if (state->should_stop && !state->has_query)
    {
        pthread_mutex_unlock(&state->mux);
        return false;
    }

    state->is_executing = true;
    pthread_mutex_unlock(&state->mux);

    return true;
}

static query_result_t execute_single_instruction(worker_state_t *state, query_context_t *ctx, int *next_pc)
{
    pthread_mutex_lock(&state->mux);
    bool eject_before_fetch = state->ejection_requested;
    pthread_mutex_unlock(&state->mux);
    
    if (eject_before_fetch)
    {
        mm_flush_all_dirty(state->memory_manager);

        t_package *res = package_create_empty(OP_WORKER_EVICT_RES);
        package_add_uint32(res, ctx->query_id);
        package_add_uint32(res, ctx->program_counter);
        package_send(res, state->master_socket);
        package_destroy(res);

        log_info(state->logger, "## Query %d: Desalojada por pedido del Master", ctx->query_id);
        return QUERY_RESULT_EJECT;
    }

    char *path = malloc(256);
    if (!path) {
        log_error(state->logger, "## Query %d: Error al asignar memoria para path", ctx->query_id);
        return QUERY_RESULT_ERROR;
    }
    
    strcpy(path, state->config->path_scripts);
    strcat(path, ctx->query_path);

    char *raw_instruction = NULL;
    if (fetch_instruction(path, ctx->program_counter, &raw_instruction) < 0)
    {
        log_error(state->logger, "## Query %d: Error en FETCH - PC: %d", 
                  ctx->query_id, ctx->program_counter);
        free(path); 
        return QUERY_RESULT_ERROR;
    }
    free(path); 

    log_info(state->logger, "## Query %d: FETCH Program Counter: %d %s", 
             ctx->query_id, ctx->program_counter, raw_instruction);

    instruction_t *instruction = malloc(sizeof(instruction_t));
    if (!instruction) {
        log_error(state->logger, "## Query %d: Error al asignar memoria para instrucción", 
                  ctx->query_id);
        free(raw_instruction);
        return QUERY_RESULT_ERROR;
    }

    if (decode_instruction(raw_instruction, instruction) < 0)
    {
        log_error(state->logger, "## Query %d: Error al decodificar - %s", 
                  ctx->query_id, raw_instruction);
        free(raw_instruction);
        free(instruction);
        return QUERY_RESULT_ERROR;
    }

    int exec_res = execute_instruction(
        instruction, state->storage_socket, state->master_socket,
        state->memory_manager, ctx->query_id, state->worker_id);

    bool end_detected = (instruction->operation == END);
    
    // Liberar instruction ANTES de retornar
    free_instruction(instruction);
    free(instruction);

    if (exec_res < 0)
    {
        log_error(state->logger, "## Query %d: Falló la instrucción - %s", 
                  ctx->query_id, raw_instruction);
        free(raw_instruction);
        return QUERY_RESULT_ERROR;  // El caller se encarga de notificar a Master
    }

    log_info(state->logger, "## Query %d: Instrucción realizada: %s", 
             ctx->query_id, raw_instruction);

    free(raw_instruction);
    *next_pc = ctx->program_counter + 1;

    pthread_mutex_lock(&state->mux);
    bool eject_after_execute = state->ejection_requested;
    pthread_mutex_unlock(&state->mux);

    if (eject_after_execute)
    {
        mm_flush_all_dirty(state->memory_manager);

        t_package *res = package_create_empty(OP_WORKER_EVICT_RES);
        package_add_uint32(res, ctx->query_id);
        package_add_uint32(res, *next_pc);  // Envío el PC actualizado
        package_send(res, state->master_socket);
        package_destroy(res);

        log_info(state->logger, "## Query %d: Desalojada por pedido del Master - PC=%d", 
                 ctx->query_id, *next_pc);
        return QUERY_RESULT_EJECT;
    }

    if (end_detected)
    {
        *next_pc = -1;
        return QUERY_RESULT_END;
    }

    return QUERY_RESULT_OK;
}

// Notificar error a Master
static void notify_master_query_error(worker_state_t *state, int query_id, int pc)
{
    if (!state || state->master_socket < 0) {
        return;
    }

    // Enviar notificación de error al Master
    t_package *error_pkg = package_create_empty(OP_WORKER_END_QUERY);
    if (error_pkg) {
        package_add_uint32(error_pkg, state->worker_id);
        package_add_uint32(error_pkg, query_id);
        
        if (package_send(error_pkg, state->master_socket) != 0) {
            log_error(state->logger, 
                      "## Error al notificar a Master sobre fallo de Query %d", 
                      query_id);
        } else {
            log_debug(state->logger, 
                      "## Master notificado sobre error en Query %d (PC=%d)", 
                      query_id, pc);
        }
        
        package_destroy(error_pkg);
    }
}
