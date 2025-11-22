#include <criterion/criterion.h>
#include "../../../src/init_master.h"
#include "../../../src/query_control_manager.h"
#include "../../../src/worker_manager.h"
#include "../../../src/disconnection_handler.h"
#include "../../helpers/test_helpers.h"

Test(cleanup, cleanup_query_resources_frees_memory) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear query
    t_query_control_block *qcb = create_query(master, 0, "/test.qry", 5, 100);
    cr_assert_not_null(qcb);
    cr_assert_eq(list_size(master->queries_table->query_list), 1);
    
    // Cleanup debería liberar TODO
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    cleanup_query_resources(qcb, master);
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    // Verificar que se removió de todas las listas
    cr_assert_eq(list_size(master->queries_table->query_list), 0);
    cr_assert_eq(list_size(master->queries_table->ready_queue), 0);
    
    // Valgrind debería confirmar que no hay leaks
    destroy_fake_master(master);
}

Test(cleanup, cleanup_worker_resources_frees_memory) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear worker
    t_worker_control_block *wcb = create_worker(master->workers_table, "1", 100);
    cr_assert_not_null(wcb);
    cr_assert_eq(list_size(master->workers_table->worker_list), 1);
    
    // Cleanup debería liberar TODO
    pthread_mutex_lock(&master->workers_table->worker_table_mutex);
    cleanup_worker_resources(wcb, master);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
    
    // Verificar que se removió de todas las listas
    cr_assert_eq(list_size(master->workers_table->worker_list), 0);
    cr_assert_eq(list_size(master->workers_table->idle_list), 0);
    
    destroy_fake_master(master);
}

Test(cleanup, destroy_master_with_active_queries) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear múltiples queries sin limpiar
    create_query(master, 0, "/q1.qry", 5, 100);
    create_query(master, 1, "/q2.qry", 3, 101);
    create_query(master, 2, "/q3.qry", 8, 102);
    
    cr_assert_eq(list_size(master->queries_table->query_list), 3);
    
    // destroy_master debería liberar TODO sin leaks
    destroy_fake_master(master);
    // Valgrind confirmará
}

Test(cleanup, destroy_master_with_active_workers) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear múltiples workers sin limpiar
    create_worker(master->workers_table, "1", 100);
    create_worker(master->workers_table, "2", 101);
    create_worker(master->workers_table, "3", 102);
    
    cr_assert_eq(list_size(master->workers_table->worker_list), 3);
    
    // destroy_master debería liberar TODO sin leaks
    destroy_fake_master(master);
    // Valgrind confirmará
}