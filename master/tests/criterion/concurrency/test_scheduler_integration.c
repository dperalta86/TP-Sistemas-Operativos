// tests/criterion/concurrency/test_scheduler_integration.c
#include <criterion/criterion.h>
#include <pthread.h>
#include <unistd.h>
#include "../../helpers/test_helpers.h"
#include "../../../src/init_master.h"
#include "../../../src/scheduler.h"

#define NUM_QUERIES 10
#define NUM_WORKERS 2
#define WORK_TIME_US 200000   // 200ms
#define DISPATCH_INTERVAL_US 100000 // 100ms

static t_master *master = NULL;
static volatile int queries_completed = 0;
static volatile bool test_done = false;

void setup_integration(void) {
    master = init_fake_master("FIFO", 500);
    queries_completed = 0;
    test_done = false;
}

void teardown_integration(void) {
    test_done = true;
    if (master) {
        destroy_fake_master(master);
        master = NULL;
    }
}

TestSuite(scheduler_integration, .init = setup_integration, .fini = teardown_integration, .timeout = 15);

typedef struct {
    t_master *master;
    int worker_idx;
} worker_thread_arg_t;

// Thread que simula un worker procesando queries
static void *worker_simulator(void *arg) {
    worker_thread_arg_t *data = (worker_thread_arg_t *)arg;
    t_master *m = data->master;
    
    // Obtener worker de la lista
    pthread_mutex_lock(&m->workers_table->worker_table_mutex);
    if (data->worker_idx >= list_size(m->workers_table->worker_list)) {
        pthread_mutex_unlock(&m->workers_table->worker_table_mutex);
        free(data);
        return NULL;
    }
    t_worker_control_block *worker = list_get(m->workers_table->worker_list, data->worker_idx);
    pthread_mutex_unlock(&m->workers_table->worker_table_mutex);
    
    while (!test_done && queries_completed < NUM_QUERIES) {
        // Si tiene query asignada, procesarla
        if (worker->current_query_id >= 0) {
            usleep(WORK_TIME_US); // Simular trabajo
            
            // Finalizar query
            pthread_mutex_lock(&m->queries_table->query_table_mutex);
            t_query_control_block *query = NULL;
            for (int i = 0; i < list_size(m->queries_table->running_list); i++) {
                t_query_control_block *q = list_get(m->queries_table->running_list, i);
                if (q && q->query_id == worker->current_query_id) {
                    query = q;
                    break;
                }
            }
            
            if (query) {
                query->state = QUERY_STATE_COMPLETED;
                list_remove_element(m->queries_table->running_list, query);
                list_add(m->queries_table->completed_list, query);
                __sync_fetch_and_add(&queries_completed, 1);
            }
            pthread_mutex_unlock(&m->queries_table->query_table_mutex);
            
            // Liberar worker
            pthread_mutex_lock(&m->workers_table->worker_table_mutex);
            worker->current_query_id = -1;
            worker->state = WORKER_STATE_IDLE;
            list_remove_element(m->workers_table->busy_list, worker);
            list_add(m->workers_table->idle_list, worker);
            pthread_mutex_unlock(&m->workers_table->worker_table_mutex);
        }
        
        usleep(50000); // 50ms
    }
    
    free(data);
    return NULL;
}

// Thread dispatcher
static void *dispatcher_thread(void *arg) {
    t_master *m = (t_master *)arg;
    
    while (queries_completed < NUM_QUERIES) {
        try_dispatch(m);
        usleep(DISPATCH_INTERVAL_US);
    }
    
    test_done = true;
    return NULL;
}

Test(scheduler_integration, full_execution_no_deadlock) {
    // Crear queries
    for (int i = 0; i < NUM_QUERIES; i++) {
        char path[50];
        snprintf(path, sizeof(path), "/query_%d.qry", i);
        t_query_control_block *qcb = create_query(master, i, path, 5 - (i % 3), -1);
        cr_assert_not_null(qcb);
    }
    
    // Crear workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        char id[10];
        snprintf(id, sizeof(id), "%d", i);
        t_worker_control_block *wcb = create_worker(master->workers_table, id, 100 + i);
        cr_assert_not_null(wcb);
    }
    
    // Lanzar threads
    pthread_t dispatcher;
    pthread_t workers[NUM_WORKERS];
    
    cr_assert_eq(pthread_create(&dispatcher, NULL, dispatcher_thread, master), 0);
    
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_thread_arg_t *arg = malloc(sizeof(worker_thread_arg_t));
        arg->master = master;
        arg->worker_idx = i;
        cr_assert_eq(pthread_create(&workers[i], NULL, worker_simulator, arg), 0);
    }
    
    // Esperar finalización
    pthread_join(dispatcher, NULL);
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    
    // Verificaciones
    cr_assert_eq(queries_completed, NUM_QUERIES, 
                 "Expected %d completed queries, got %d", NUM_QUERIES, queries_completed);
    cr_assert_eq(list_size(master->queries_table->completed_list), NUM_QUERIES);
    cr_assert_eq(list_size(master->queries_table->ready_queue), 0);
    cr_assert_eq(list_size(master->queries_table->running_list), 0);
}