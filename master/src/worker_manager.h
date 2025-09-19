#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <pthread.h>

typedef enum {
    WORKER_STATE_DISCONNECTED,
    WORKER_STATE_IDLE,
    WORKER_STATE_BUSY
} t_worker_state;

typedef struct {
    int worker_id;
    char *ip_address; // Ver si es necesasrio guardar la IP y el puerto
    int port;
    int current_query_id;
    t_worker_state state;
} t_worker_control_block;

typedef struct {
    t_worker_control_block *worker_list; // Lista de t_worker_control_block
    int total_workers_connected; // Define el nivel de multiprocesamiento

    t_worker_control_block *idle_list; // Lista de workers en estado IDLE
    t_worker_control_block *busy_list; // Lista de workers en estado BUSY
    t_worker_control_block *disconnected_list; // Lista de workers en estado DISCONNECTED

    // Mutex para sincronización en asignación de workers
    pthread_mutex_t worker_table_mutex;
} t_worker_table;

#endif