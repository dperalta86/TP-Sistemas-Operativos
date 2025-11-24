#include <criterion/criterion.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../../../src/init_master.h"
#include "../../../src/query_control_manager.h"
#include "../../../src/worker_manager.h"
#include "../../../src/scheduler.h"
#include "../../helpers/test_helpers.h"

#define NUM_WORKERS 2
#define NUM_QUERIES 4
#define WORK_SIM_MS 200

typedef struct {
    int query_id;
    int priority;
    int assigned_worker_id;
    uint64_t end_time;
} t_query_execution_log;

static t_query_execution_log log_exec[NUM_QUERIES];
static _Atomic int queries_completed = 0;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int worker_index;
    t_master *master;
} t_thread_args;

/* -------------------------------------------------------------------------- */
/*                               WORKER THREAD                                */
/* -------------------------------------------------------------------------- */

void *worker_simulator(void *raw_args) {
    t_thread_args *args = raw_args;
    int idx = args->worker_index;
    t_master *master = args->master;
    free(args);

    while (atomic_load(&queries_completed) < NUM_QUERIES) {

        pthread_mutex_lock(&master->workers_table->worker_table_mutex);

        if (idx >= list_size(master->workers_table->worker_list)) {
            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
            break;
        }

        t_worker_control_block *worker = list_get(master->workers_table->worker_list, idx);

        if (worker->state == WORKER_STATE_BUSY && worker->current_query_id >= 0) {
            int qid = worker->current_query_id;
            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

            /* simular trabajo */
            usleep(WORK_SIM_MS * 1000);

            pthread_mutex_lock(&master->queries_table->query_table_mutex);
            pthread_mutex_lock(&master->workers_table->worker_table_mutex);

            /* buscar QCB */
            t_query_control_block *qcb = NULL;
            for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
                t_query_control_block *q = list_get(master->queries_table->running_list, i);
                if (q && q->query_id == qid) {
                    qcb = q;
                    break;
                }
            }

            if (qcb) {
                /* registrar log */
                pthread_mutex_lock(&log_mutex);
                int slot = atomic_load(&queries_completed);
                log_exec[slot].query_id = qcb->query_id;
                log_exec[slot].priority = qcb->priority;
                log_exec[slot].assigned_worker_id = worker->worker_id;
                log_exec[slot].end_time = now_ms_monotonic();
                atomic_fetch_add(&queries_completed, 1);
                pthread_mutex_unlock(&log_mutex);

                /* mover a completed */
                qcb->state = QUERY_STATE_COMPLETED;
                list_remove_element(master->queries_table->running_list, qcb);
                list_add(master->queries_table->completed_list, qcb);
            }

            /* liberar worker */
            worker->current_query_id = -1;
            worker->state = WORKER_STATE_IDLE;
            list_remove_element(master->workers_table->busy_list, worker);
            list_add(master->workers_table->idle_list, worker);

            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
            pthread_mutex_unlock(&master->queries_table->query_table_mutex);

        } else {
            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        }

        usleep(50000);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */
/*                           DISPATCHER THREAD                                */
/* -------------------------------------------------------------------------- */

void *dispatcher_thread(void *arg) {
    t_master *master = arg;

    while (atomic_load(&queries_completed) < NUM_QUERIES) {
        try_dispatch(master);
        usleep(100000);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */
/*                                    TEST                                    */
/* -------------------------------------------------------------------------- */

Test(fifo_scheduling, four_queries_two_workers, .timeout = 15) {

    t_master *master = init_fake_master("FIFO", 5000);

    atomic_store(&queries_completed, 0);
    memset(log_exec, 0, sizeof(log_exec));

    /* crear workers */
    create_worker(master->workers_table, "0", 10);
    create_worker(master->workers_table, "1", 11);

    cr_assert_eq(list_size(master->workers_table->worker_list), NUM_WORKERS);

    /* crear queries */
    int prios[] = {4, 3, 5, 1};

    for (int i = 0; i < NUM_QUERIES; i++) {
        char path[32];
        sprintf(path, "/F_%d.qry", i);
        t_query_control_block *q = create_query(master, i, path, prios[i], 200 + i);
        cr_assert_not_null(q);
    }

    cr_assert_eq(list_size(master->queries_table->ready_queue), NUM_QUERIES);

    /* lanzar dispatcher */
    pthread_t disp;
    pthread_create(&disp, NULL, dispatcher_thread, master);

    /* lanzar workers */
    pthread_t workers[NUM_WORKERS];

    for (int i = 0; i < NUM_WORKERS; i++) {
        t_thread_args *args = malloc(sizeof(t_thread_args));
        args->worker_index = i;
        args->master = master;
        pthread_create(&workers[i], NULL, worker_simulator, args);
    }

    /* esperar */
    pthread_join(disp, NULL);
    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_join(workers[i], NULL);

    /* asserts */
    cr_assert_eq(atomic_load(&queries_completed), NUM_QUERIES);
    cr_assert_eq(list_size(master->queries_table->completed_list), NUM_QUERIES);

    /* FIFO group check */

    bool first_batch_ok = false;

    /* primeras dos deben ser Q0 y Q1 (en cualquier orden) */
    int qA = log_exec[0].query_id;
    int qB = log_exec[1].query_id;

    if ((qA == 0 || qA == 1) && (qB == 0 || qB == 1))
        first_batch_ok = true;

    cr_assert(first_batch_ok, "Q0 y Q1 deben ejecutarse primero");

    destroy_fake_master(master);
}

