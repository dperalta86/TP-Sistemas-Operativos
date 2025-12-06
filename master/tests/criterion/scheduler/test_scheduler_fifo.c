// test_scheduler_fifo.c
#include <criterion/criterion.h>
#include "scheduler.h"
#include "../../helpers/test_helpers.h"

Test(scheduler_fifo, dispatch_single_query_to_idle_worker) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear worker idle
    t_worker_control_block *worker = create_worker(master->workers_table, "1", 100);
    
    // Crear query en ready
    t_query_control_block *query = create_query(master, 0, "/q1.txt", 5, 10);
    
    // Intentar dispatch
    int result = try_dispatch(master);
    
    cr_assert_eq(result, 0);
    cr_assert_eq(list_size(master->queries_table->ready_queue), 0);
    cr_assert_eq(list_size(master->queries_table->running_list), 1);
    cr_assert_eq(list_size(master->workers_table->idle_list), 0);
    cr_assert_eq(list_size(master->workers_table->busy_list), 1);
    cr_assert_eq(worker->state, WORKER_STATE_BUSY);
    cr_assert_eq(query->state, QUERY_STATE_RUNNING);
    cr_assert_eq(worker->current_query_id, query->query_id);
    
    destroy_fake_master(master);
    log_destroy(logger);
}

Test(scheduler_fifo, dispatch_multiple_queries_fifo_order) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear 3 workers
    create_worker(master->workers_table, "1", 100);
    create_worker(master->workers_table, "2", 101);
    create_worker(master->workers_table, "3", 102);
    
    // Crear 5 queries (más que workers)
    t_query_control_block *q1 = create_query(master, 0, "/q1.txt", 5, 10);
    t_query_control_block *q2 = create_query(master, 1, "/q2.txt", 3, 11);
    t_query_control_block *q3 = create_query(master, 2, "/q3.txt", 8, 12);
    t_query_control_block *q4 = create_query(master, 3, "/q4.txt", 1, 13);
    t_query_control_block *q5 = create_query(master, 4, "/q5.txt", 7, 14);
    
    // Primer dispatch: deben asignarse q1, q2, q3
    try_dispatch(master);
    try_dispatch(master);
    try_dispatch(master);
    
    cr_assert_eq(list_size(master->queries_table->running_list), 3);
    cr_assert_eq(list_size(master->queries_table->ready_queue), 2);
    cr_assert_eq(list_size(master->workers_table->busy_list), 3);
    
    // Verificar que q1, q2, q3 están en running (FIFO, sin importar prioridad)
    cr_assert_eq(q1->state, QUERY_STATE_RUNNING);
    cr_assert_eq(q2->state, QUERY_STATE_RUNNING);
    cr_assert_eq(q3->state, QUERY_STATE_RUNNING);
    cr_assert_eq(q4->state, QUERY_STATE_READY);
    cr_assert_eq(q5->state, QUERY_STATE_READY);
    
    destroy_fake_master(master);
    log_destroy(logger);
}

Test(scheduler_fifo, no_dispatch_without_workers) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear queries pero sin workers
    create_query(master, 0, "/q1.txt", 5, 10);
    
    int result = try_dispatch(master);
    
    cr_assert_eq(result, 0); // No hay error, solo no hace nada
    cr_assert_eq(list_size(master->queries_table->ready_queue), 1);
    cr_assert_eq(list_size(master->queries_table->running_list), 0);
    
    destroy_fake_master(master);
    log_destroy(logger);
}