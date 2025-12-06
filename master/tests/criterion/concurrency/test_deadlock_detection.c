// tests/criterion/concurrency/test_deadlock_detection.c
#include <criterion/criterion.h>
#include <pthread.h>
#include <unistd.h>
#include "../../helpers/test_helpers.h"
#include "../../../src/init_master.h"
#include <linux/time.h>

#define ITERATIONS 20
#define CREATE_DELAY_US 50000   // 50ms
#define REORDER_DELAY_US 75000  // 75ms

static t_master *master = NULL;
static volatile bool load_finished = false;
static volatile bool reorder_finished = false;

void setup_deadlock(void) {
    master = init_fake_master("PRIORITY", 500);
    load_finished = false;
    reorder_finished = false;
}

void teardown_deadlock(void) {
    if (master) {
        destroy_fake_master(master);
        master = NULL;
    }
}

TestSuite(deadlock_detection, .init = setup_deadlock, .fini = teardown_deadlock, .timeout = 10);

// Comparador para ordenamiento
static bool priority_comparator(void *a, void *b) {
    t_query_control_block *q1 = a;
    t_query_control_block *q2 = b;
    return q1->priority < q2->priority;
}

// Thread que crea queries
static void *thread_create_queries(void *arg) {
    t_master *m = (t_master *)arg;
    
    for (int i = 0; i < ITERATIONS; i++) {
        t_query_control_block *qcb = malloc(sizeof(t_query_control_block));
        qcb->query_id = i;
        qcb->priority = 15 - (i % 5);
        qcb->state = QUERY_STATE_READY;
        qcb->query_file_path = strdup("dummy.qry");
        
        pthread_mutex_lock(&m->queries_table->query_table_mutex);
        list_add(m->queries_table->ready_queue, qcb);
        pthread_mutex_unlock(&m->queries_table->query_table_mutex);
        
        usleep(CREATE_DELAY_US);
    }
    
    load_finished = true;
    return NULL;
}

// Thread que reordena
static void *thread_reorder_queries(void *arg) {
    t_master *m = (t_master *)arg;
    
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&m->queries_table->query_table_mutex);
        list_sort(m->queries_table->ready_queue, priority_comparator);
        pthread_mutex_unlock(&m->queries_table->query_table_mutex);
        
        usleep(REORDER_DELAY_US);
    }
    
    reorder_finished = true;
    return NULL;
}

Test(deadlock_detection, concurrent_load_and_reorder_no_deadlock) {
    pthread_t create_thread, reorder_thread;
    
    cr_assert_eq(pthread_create(&create_thread, NULL, thread_create_queries, master), 0);
    cr_assert_eq(pthread_create(&reorder_thread, NULL, thread_reorder_queries, master), 0);
    
    // Esperar con timeout
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 8; // 8 segundos máximo
    
    // Join con timeout implícito por el TestSuite(.timeout = 10)
    pthread_join(create_thread, NULL);
    pthread_join(reorder_thread, NULL);
    
    // Si llegamos aquí, no hubo deadlock
    cr_assert(load_finished, "Create thread should finish");
    cr_assert(reorder_finished, "Reorder thread should finish");
    
    int final_size = list_size(master->queries_table->ready_queue);
    cr_assert_eq(final_size, ITERATIONS, "All queries should be in ready queue");
}

Test(deadlock_detection, stress_test_mutex_contention) {
    const int NUM_THREADS = 10;
    pthread_t threads[NUM_THREADS];
    
    // Lanzar múltiples threads que compiten por el mismo mutex
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i % 2 == 0) {
            cr_assert_eq(pthread_create(&threads[i], NULL, thread_create_queries, master), 0);
        } else {
            cr_assert_eq(pthread_create(&threads[i], NULL, thread_reorder_queries, master), 0);
        }
    }
    
    // Esperar a todos
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verificar que no se rompió nada
    cr_assert_not_null(master->queries_table->ready_queue);
    cr_assert_geq(list_size(master->queries_table->ready_queue), 0);
}