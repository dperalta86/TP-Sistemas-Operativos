// tests/criterion/concurrency/test_aging_concurrent.c
#include <criterion/criterion.h>
#include <unistd.h>
#include "../../helpers/test_helpers.h"
#include "../../../src/init_master.h"
#include "../../../src/aging.h"
#include "../../../src/query_control_manager.h"

Test(aging_concurrent, priority_decreases_after_interval, .timeout = 10) {
    // Setup inline
    t_master *master = init_fake_master("PRIORITY", 500);
    master->running = true;
    pthread_create(&master->aging_thread, NULL, aging_thread_func, master);
    
    // Crear query
    t_query_control_block *qcb = create_query(master, 1, "/test.qry", 3, -1);
    cr_assert_not_null(qcb);
    cr_assert_eq(qcb->priority, 3);
    
    // Esperar más de un intervalo
    usleep(600 * 1000); // 600ms
    
    // Verificar prioridad
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    int final_priority = qcb->priority;
    list_remove_element(master->queries_table->ready_queue, qcb);
    list_remove_element(master->queries_table->query_list, qcb);
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    cr_assert_eq(final_priority, 2);
    
    // Cleanup inline
    master->running = false;
    pthread_join(master->aging_thread, NULL);
    
    if (qcb->query_file_path) free(qcb->query_file_path);
    free(qcb);
    
    destroy_fake_master(master);
}

Test(aging_concurrent, priority_never_negative, .timeout = 10) {
    t_master *master = init_fake_master("PRIORITY", 500);
    master->running = true;
    pthread_create(&master->aging_thread, NULL, aging_thread_func, master);
    
    t_query_control_block *qcb = create_query(master, 2, "/test.qry", 1, -1);
    cr_assert_not_null(qcb);
    
    usleep(1500 * 1000); // 1.5 segundos
    
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    int final_priority = qcb->priority;
    list_remove_element(master->queries_table->ready_queue, qcb);
    list_remove_element(master->queries_table->query_list, qcb);
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    cr_assert_geq(final_priority, 0);
    cr_assert_eq(final_priority, 0);
    
    master->running = false;
    pthread_join(master->aging_thread, NULL);
    
    if (qcb->query_file_path) free(qcb->query_file_path);
    free(qcb);
    
    destroy_fake_master(master);
}

Test(aging_concurrent, multiple_queries_age_correctly, .timeout = 10) {
    t_master *master = init_fake_master("PRIORITY", 500);
    master->running = true;
    pthread_create(&master->aging_thread, NULL, aging_thread_func, master);
    
    int initial_priorities[] = {5, 3, 8, 2, 10};
    t_query_control_block *queries[5];
    
    for (int i = 0; i < 5; i++) {
        queries[i] = create_query(master, i, "/test.qry", initial_priorities[i], -1);
        cr_assert_not_null(queries[i]);
    }
    
    usleep(600 * 1000);
    
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    for (int i = 0; i < 5; i++) {
        int expected = (initial_priorities[i] > 0) ? initial_priorities[i] - 1 : 0;
        cr_assert_eq(queries[i]->priority, expected);
    }
    
    for (int i = 0; i < 5; i++) {
        list_remove_element(master->queries_table->ready_queue, queries[i]);
        list_remove_element(master->queries_table->query_list, queries[i]);
    }
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    master->running = false;
    pthread_join(master->aging_thread, NULL);
    
    for (int i = 0; i < 5; i++) {
        if (queries[i]->query_file_path) free(queries[i]->query_file_path);
        free(queries[i]);
    }
    
    destroy_fake_master(master);
}