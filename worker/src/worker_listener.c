#include "worker_listener.h"
#include <string.h>
#include <stdlib.h>
#include <utils/logger.h>
#include <connections/master.h>

static void assign_query(t_package *pkg, worker_state_t *state);
static void eject_query(t_package *pkg, worker_state_t *state);
static void end_query(t_package *pkg, worker_state_t *state);

void *master_listener_thread(void *arg)
{
    worker_state_t *state = (worker_state_t *)arg;

    while (true)
    {
        t_package *pkg = package_receive(state->master_socket);
        if (!pkg)
        {
            log_error(state->logger, "## Se perdió la conexión con Master");
            break;
        }

        switch (pkg->operation_code)
        {
        case OP_ASSIGN_QUERY:
        {
            assign_query(pkg, state);
            break;
        }

        case OP_EJECT_QUERY:
        {
            eject_query(pkg, state);
            break;
        }

        case OP_END_QUERY:
            end_query(pkg, state);
            return NULL;

        default:
            log_warning(state->logger, "## Código de operación desconocido: %d", pkg->operation_code);
            break;
        }

        package_destroy(pkg);
    }

    return NULL;
}

static void assign_query(t_package *pkg, worker_state_t *state)
{
    uint32_t query_id, program_counter;
    char *path = NULL;

    if (!package_read_uint32(pkg, &query_id))
        return;
    if (!package_read_uint32(pkg, &program_counter))
        return;
    path = package_read_string(pkg);
    if (!path)
        return;

    pthread_mutex_lock(&state->mux);
    strcpy(state->current_query.query_path, path);
    state->current_query.program_counter = program_counter;
    state->current_query.query_id = query_id;
    state->has_query = true;
    state->should_stop = false;

    mm_set_query_id(state->memory_manager, query_id);

    pthread_mutex_unlock(&state->mux);
    pthread_cond_signal(&state->new_query_cond);

    log_info(state->logger, "## Se asignó Query %d - Program Counter=%d - Ruta=%s", query_id, program_counter, path);
    free(path);
}

static void eject_query(t_package *pkg, worker_state_t *state)
{
    uint32_t query_id;
    if (!package_read_uint32(pkg, &query_id))
        return;

    pthread_mutex_lock(&state->mux);
    if (state->has_query && state->current_query.query_id == query_id) {
        if (state->is_executing) {
            state->ejection_requested = true;
        } else {
            state->has_query = false;
            int pc = state->current_query.program_counter;
            pthread_mutex_unlock(&state->mux);

            mm_flush_all_dirty(state->memory_manager);
            
            t_package *resp = package_create_empty(OP_WORKER_EVICT_RES);
            package_add_uint32(resp, query_id);
            package_add_uint32(resp, pc);
            package_send(resp, state->master_socket);
            package_destroy(resp);
            log_info(state->logger, "## Query %d: Desalojada en READY, PC=%d", query_id, pc);
            return;
        }
    }
    pthread_mutex_unlock(&state->mux);

    log_info(state->logger, "## Se recibió solicitud de desalojo para Query %d", query_id);
}

static void end_query(t_package *pkg, worker_state_t *state)
{
    log_info(state->logger, "## Se recibió solicitud de finalización del Worker");
    pthread_mutex_lock(&state->mux);
    state->should_stop = true;
    state->has_query = false;
    pthread_mutex_unlock(&state->mux);
    package_destroy(pkg);
}
