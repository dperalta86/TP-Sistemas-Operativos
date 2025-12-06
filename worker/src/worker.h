#ifndef WORKER_H
#define WORKER_H

#include <limits.h>
#include <stdbool.h>
#include <pthread.h>
#include <memory/memory_manager.h>
#include <config/worker_config.h>
#include <commons/log.h>

typedef struct
{
    char query_path[PATH_MAX];
    int query_id;
    int program_counter;
} query_context_t;


typedef struct
{
    // --- Concurrencia y Sincronización ---
    pthread_mutex_t mux;
    pthread_cond_t new_query_cond;
    bool has_query;
    bool should_stop;
    bool ejection_requested;
    bool is_executing;
    query_context_t current_query;

    // --- Recursos del Worker ---
    int master_socket;
    int storage_socket;
    t_worker_config *config;
    t_log *logger;
    memory_manager_t *memory_manager;
    int worker_id;
} worker_state_t;

#endif
