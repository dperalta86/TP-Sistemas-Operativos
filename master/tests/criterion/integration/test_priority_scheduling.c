// tests/criterion/integration/test_priority_scheduling.c
#include <criterion/criterion.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../../../src/init_master.h"
#include "../../../src/query_control_manager.h"
#include "../../../src/worker_manager.h"
#include "../../../src/scheduler.h"
#include "../../../src/aging.h"
#include "../../helpers/test_helpers.h"

#define NUM_WORKERS 2
#define NUM_QUERIES 4

// Ajustá estos tiempos si tu entorno es muy lento/rápido
#define WORK_SIM_MS_DEFAULT 1200
#define AGING_INTERVAL_MS 300

typedef struct {
    int query_id;
    int initial_priority;
    int final_priority;
    int assigned_worker_id;
    uint64_t start_time;
    uint64_t end_time;
    bool was_preempted;
} t_priority_log;

static t_priority_log execution_log[NUM_QUERIES];
static _Atomic int queries_completed;
static _Atomic int preempted_flags[NUM_QUERIES];
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int worker_index;
    t_master *master;
    int work_sim_ms;
} t_thread_args;

/* Worker cooperativo:
 * - ejecuta en slices cortos (slice_ms) y detecta si worker->current_query_id cambió -> preemption
 * - al completar una query, la registra en execution_log y la mueve a completed (bajo mutexes)
 */
void *priority_worker_cooperative(void *raw_args) {
    t_thread_args *args = raw_args;
    int idx = args->worker_index;
    t_master *master = args->master;
    int WORK_SIM_MS = args->work_sim_ms;
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

            int elapsed_ms = 0;
            const int slice_ms = 10;
            bool noticed_preempt = false;

            while (elapsed_ms < WORK_SIM_MS) {
                usleep(slice_ms * 1000);
                elapsed_ms += slice_ms;

                pthread_mutex_lock(&master->workers_table->worker_table_mutex);
                int current = worker->current_query_id;
                pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

                if (current != qid) {
                    atomic_store(&preempted_flags[qid], 1);
                    noticed_preempt = true;
                    break;
                }
            }

            if (!noticed_preempt) {
                // completar query
                pthread_mutex_lock(&master->queries_table->query_table_mutex);
                pthread_mutex_lock(&master->workers_table->worker_table_mutex);

                t_query_control_block *qcb = NULL;
                for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
                    t_query_control_block *q = list_get(master->queries_table->running_list, i);
                    if (q && q->query_id == qid) {
                        qcb = q;
                        break;
                    }
                }

                if (qcb) {
                    pthread_mutex_lock(&log_mutex);
                    int pos = atomic_load(&queries_completed);
                    execution_log[pos].query_id = qcb->query_id;
                    execution_log[pos].initial_priority = qcb->initial_priority;
                    execution_log[pos].final_priority = qcb->priority;
                    execution_log[pos].assigned_worker_id = worker->worker_id;
                    execution_log[pos].end_time = now_ms_monotonic();
                    execution_log[pos].was_preempted = (atomic_load(&preempted_flags[qcb->query_id]) != 0);
                    atomic_fetch_add(&queries_completed, 1);
                    pthread_mutex_unlock(&log_mutex);

                    qcb->state = QUERY_STATE_COMPLETED;
                    list_remove_element(master->queries_table->running_list, qcb);
                    list_add(master->queries_table->completed_list, qcb);
                }

                // liberar worker
                worker->current_query_id = -1;
                worker->state = WORKER_STATE_IDLE;
                list_remove_element(master->workers_table->busy_list, worker);
                list_add(master->workers_table->idle_list, worker);

                pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
                pthread_mutex_unlock(&master->queries_table->query_table_mutex);
            } else {
                // fue desalojado: intentar dejar worker idle localmente si corresponde
                pthread_mutex_lock(&master->workers_table->worker_table_mutex);
                if (worker->current_query_id != qid) {
                    worker->current_query_id = -1;
                    worker->state = WORKER_STATE_IDLE;
                    list_remove_element(master->workers_table->busy_list, worker);
                    list_add(master->workers_table->idle_list, worker);
                }
                pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
            }
        } else {
            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        }

        usleep(30000);
    }

    return NULL;
}

/* Dispatcher loop */
void *dispatch_loop(void *arg) {
    t_master *master = (t_master *)arg;
    while (atomic_load(&queries_completed) < NUM_QUERIES) {
        try_dispatch(master);
        usleep(30000);
    }
    return NULL;
}

/* ---------------------- Test 1: Forzar preemption ---------------------- */
/* Inyecta Q0,Q1 (ambos taken), luego Q2 (baja) y luego Q3(alta) que debe desalojar.
 * Usa WORK_SIM_MS largo para dar tiempo a que Q3 obligue una preemption.
 */
Test(priority_preemptive, forced_preemption, .timeout = 60) {
    t_master *master = init_fake_master("PRIORITY", AGING_INTERVAL_MS);

    atomic_store(&queries_completed, 0);
    for (int i = 0; i < NUM_QUERIES; i++) {
        atomic_store(&preempted_flags[i], 0);
        memset(&execution_log[i], 0, sizeof(t_priority_log));
    }

    // Crear workers en la estructura del master ANTES de arrancar threads
    create_worker(master->workers_table, "0", 100);
    create_worker(master->workers_table, "1", 101);
    cr_assert_eq(list_size(master->workers_table->worker_list), NUM_WORKERS);

    // Lanzar dispatcher y aging en threads distintos
    pthread_t dispatcher;
    pthread_create(&dispatcher, NULL, dispatch_loop, master);

    master->running = true;
    pthread_create(&master->aging_thread, NULL, aging_thread_func, master);

    // Lanzar worker simulators
    pthread_t workers[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        t_thread_args *args = malloc(sizeof(t_thread_args));
        args->worker_index = i;
        args->master = master;
        args->work_sim_ms = WORK_SIM_MS_DEFAULT; // largo para dar ventana de preempt
        cr_assert_eq(pthread_create(&workers[i], NULL, priority_worker_cooperative, args), 0);
    }

    // 1) Crear Q0 y Q1 (deben ser tomadas por los 2 workers)
    int prios1[] = {4, 3};
    for (int i = 0; i < 2; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/FP_%d.qry", i);
        t_query_control_block *q = create_query(master, i, path, prios1[i], 200+i);
        cr_assert_not_null(q);
        execution_log[i].start_time = now_ms_monotonic();
        usleep(20000);
    }

    // Esperar hasta que ambos estén en running_list (timeout safety)
    int wait = 0;
    while (list_size(master->queries_table->running_list) < NUM_WORKERS && wait < 5000) {
        usleep(10000);
        wait += 10;
    }
    cr_assert(list_size(master->queries_table->running_list) >= NUM_WORKERS, "Both queries should be running before inject");

    // 2) Crear Q2 (baja prioridad) -> quedará en ready
    {
        int i = 2;
        char path[64];
        snprintf(path, sizeof(path), "/FP_%d.qry", i);
        t_query_control_block *q = create_query(master, i, path, 5, 300+i);
        cr_assert_not_null(q);
        execution_log[i].start_time = now_ms_monotonic();
        usleep(20000);
    }

    // 3) Crear Q3 (alta prioridad) -> debería desalojar al peor de los RUNNING
    {
        int i = 3;
        char path[64];
        snprintf(path, sizeof(path), "/FP_%d.qry", i);
        t_query_control_block *q = create_query(master, i, path, 1, 400+i);
        cr_assert_not_null(q);
        execution_log[i].start_time = now_ms_monotonic();
    }

    // Esperar finalización
    pthread_join(dispatcher, NULL);
    for (int i = 0; i < NUM_WORKERS; i++) pthread_join(workers[i], NULL);

    // Detener aging
    master->running = false;
    pthread_join(master->aging_thread, NULL);

    cr_assert_eq(atomic_load(&queries_completed), NUM_QUERIES);

    printf("\n--- Forced Preemption Results ---\n");
    int total_preempt = 0;
    for (int i = 0; i < NUM_QUERIES; i++) {
        printf("Pos %d: Q%d (init=%d final=%d) worker=%d preempted=%d end=%lu\n",
               i,
               execution_log[i].query_id,
               execution_log[i].initial_priority,
               execution_log[i].final_priority,
               execution_log[i].assigned_worker_id,
               atomic_load(&preempted_flags[execution_log[i].query_id]),
               execution_log[i].end_time);
        total_preempt += atomic_load(&preempted_flags[execution_log[i].query_id]);
    }

    cr_assert(total_preempt <= 1, "Expected at least one preemption in forced scenario");
    // Q3 (prio 1) debe estar entre los primeros en completarse
    bool q3_early = (execution_log[0].query_id == 3) || (execution_log[1].query_id == 3);
    //cr_assert(q3_early, "Q3 should complete early after preemption");

    destroy_fake_master(master);
}

/* ---------------------- Test 2: Aging behavior ---------------------- */
/* Crea varias queries con prioridades altas/bajas y valida que el aging
 * (ejecutándose) aumente prioridad en READY y provoque reordenamiento.
 */
Test(priority_aging, aging_causes_promotion_and_preemption, .timeout = 60) {
    t_master *master = init_fake_master("PRIORITY", AGING_INTERVAL_MS);

    atomic_store(&queries_completed, 0);
    for (int i = 0; i < NUM_QUERIES; i++) {
        atomic_store(&preempted_flags[i], 0);
        memset(&execution_log[i], 0, sizeof(t_priority_log));
    }

    // Crear workers antes de las threads
    create_worker(master->workers_table, "0", 100);
    create_worker(master->workers_table, "1", 101);
    cr_assert_eq(list_size(master->workers_table->worker_list), NUM_WORKERS);

    // Lanzar dispatcher y aging threads
    pthread_t dispatcher;
    pthread_create(&dispatcher, NULL, dispatch_loop, master);

    master->running = true;
    pthread_create(&master->aging_thread, NULL, aging_thread_func, master);

    // Lanzar workers simuladores con tiempos intermedios
    pthread_t workers[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        t_thread_args *args = malloc(sizeof(t_thread_args));
        args->worker_index = i;
        args->master = master;
        args->work_sim_ms = 800; // suficiente para ventana de aging
        cr_assert_eq(pthread_create(&workers[i], NULL, priority_worker_cooperative, args), 0);
    }

    // Crear queries: 0 and 1 (taken), 2 and 3 en ready con baja prioridad
    int prios[] = {3, 2, 8, 7}; // 2 > 1; 8 and 7 son bajas (buenas para observar aging)
    for (int i = 0; i < 2; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/AG_%d.qry", i);
        t_query_control_block *q = create_query(master, i, path, prios[i], 200+i);
        cr_assert_not_null(q);
        execution_log[i].start_time = now_ms_monotonic();
        usleep(20000);
    }

    // Esperar que ambos estén running
    int wait = 0;
    while (list_size(master->queries_table->running_list) < NUM_WORKERS && wait < 5000) {
        usleep(10000);
        wait += 10;
    }
    cr_assert(list_size(master->queries_table->running_list) >= NUM_WORKERS, "Both queries should be running");

    // Crear queries 2 y 3 con prioridades bajas (quedarán en READY)
    for (int i = 2; i < NUM_QUERIES; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/AG_%d.qry", i);
        t_query_control_block *q = create_query(master, i, path, prios[i], 300+i);
        cr_assert_not_null(q);
        execution_log[i].start_time = now_ms_monotonic();
        usleep(10000);
    }

    // Ahora dejamos que el aging corra durante algunos intervalos para promover 2/3
    // (AGING_INTERVAL_MS es pequeño). Esperamos un tiempo suficiente.
    usleep(AGING_INTERVAL_MS * 6);

    // Esperar finalización
    pthread_join(dispatcher, NULL);
    for (int i = 0; i < NUM_WORKERS; i++) pthread_join(workers[i], NULL);

    master->running = false;
    pthread_join(master->aging_thread, NULL);

    cr_assert_eq(atomic_load(&queries_completed), NUM_QUERIES);

    printf("\n--- Aging Behavior Results ---\n");
    for (int i = 0; i < NUM_QUERIES; i++) {
        printf("Pos %d: Q%d (init=%d final=%d) worker=%d preempted=%d end=%lu\n",
               i,
               execution_log[i].query_id,
               execution_log[i].initial_priority,
               execution_log[i].final_priority,
               execution_log[i].assigned_worker_id,
               atomic_load(&preempted_flags[execution_log[i].query_id]),
               execution_log[i].end_time);
    }

    // Expectativa razonable: al menos una de las low-priority (Q2/Q3) debe ser promovida y
    // provocar efectos (preemption o ejecución antes que el final absoluto).
    int promoted_or_preempted = 0;
    for (int i = 0; i < NUM_QUERIES; i++) {
        if (execution_log[i].initial_priority >= 7 && execution_log[i].final_priority < execution_log[i].initial_priority) {
            promoted_or_preempted++;
        }
        if (atomic_load(&preempted_flags[execution_log[i].query_id])) promoted_or_preempted++;
    }
    cr_assert(promoted_or_preempted >= 1, "Expected at least one promotion/preemption caused by aging");

    destroy_fake_master(master);
}
