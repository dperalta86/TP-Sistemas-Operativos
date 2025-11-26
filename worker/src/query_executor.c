#include "query_executor.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <utils/logger.h>
#include <query_interpreter/query_interpreter.h>

static bool fetch_next_query(worker_state_t *state);
static query_result_t execute_single_instruction(worker_state_t *state, query_context_t *ctx, int *next_pc);

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
            }

            state->has_query = false;
            state->is_executing = false;
            log_info(state->logger, "## Query %d: %s", ctx.query_id, (result == QUERY_RESULT_END ? "Finalizada" : "Abortada"));
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
        return false; // Detener el hilo
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

    char *raw_instruction = NULL;
    char *path = NULL;
    path = (char *)malloc(256);
    strcpy(path, state->config->path_scripts);
    strcat(path, ctx->query_path);
    if (fetch_instruction(path, ctx->program_counter, &raw_instruction) < 0)
    {
        log_error(state->logger, "## Query %d: Error en FETCH - PC: %d", ctx->query_id, ctx->program_counter);
        return QUERY_RESULT_ERROR;
    }

    log_info(state->logger, "## Query %d: FETCH Program Counter: %d %s", ctx->query_id, ctx->program_counter, raw_instruction);

    instruction_t *instruction = malloc(sizeof(instruction_t));
    if (decode_instruction(raw_instruction, instruction) < 0)
    {
        log_error(state->logger, "## Query %d: Error al decodificar - %s", ctx->query_id, raw_instruction);
        free(raw_instruction);
        free(instruction);
        return QUERY_RESULT_ERROR;
    }

    int exec_res = execute_instruction(
        instruction, state->storage_socket, state->master_socket,
        state->memory_manager, ctx->query_id, state->worker_id);

    bool end_detected = (instruction->operation == END);

    if (exec_res < 0)
    {
        log_error(state->logger, "## Query %d: Falló la instrucción - %s", ctx->query_id, raw_instruction);
        free(raw_instruction);
        return QUERY_RESULT_ERROR;
    }

    log_info(state->logger, "## Query %d: Instrucción realizada: %s", ctx->query_id, raw_instruction);

    free(raw_instruction);
    *next_pc = ctx->program_counter + 1;

    pthread_mutex_lock(&state->mux);
    bool eject_after_execute = state->ejection_requested;
    pthread_mutex_unlock(&state->mux);

    if (eject_after_execute)
    {
        // Flush de todos los File:Tag modificados antes del desalojo
        mm_flush_all_dirty(state->memory_manager);

        t_package *res = package_create_empty(OP_WORKER_EVICT_RES);
        package_add_uint32(res, ctx->query_id);
        package_add_uint32(res, ctx->program_counter);
        package_send(res, state->master_socket);
        package_destroy(res);

        log_info(state->logger, "## Query %d: Desalojada por pedido del Master - PC=%d", ctx->query_id, ctx->program_counter);
        return QUERY_RESULT_EJECT;
    }

    if (end_detected)
    {
        *next_pc = -1;
        return QUERY_RESULT_END;
    }

    return QUERY_RESULT_OK;
}
