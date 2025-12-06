#include <criterion/criterion.h>
#include <commons/collections/list.h>
#include "../../../src/init_master.h"
#include "../../../src/query_control_manager.h"
#include "../../helpers/test_helpers.h"


// ==========================================
// Tests de Gestión de Workers
// ==========================================

Test(worker_management, create_worker_basic) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    t_worker_control_block *wcb = create_worker(
        master->workers_table,
        "1",    // worker_id (string)
        100     // socket_fd
    );
    
    cr_assert_not_null(wcb);
    cr_assert_eq(wcb->worker_id, 1);
    cr_assert_eq(wcb->socket_fd, 100);
    cr_assert_eq(wcb->state, WORKER_STATE_IDLE);
    cr_assert_eq(wcb->current_query_id, -1);
    
    // Verificar que está en las listas correctas
    cr_assert_eq(list_size(master->workers_table->worker_list), 1);
    cr_assert_eq(list_size(master->workers_table->idle_list), 1);
    cr_assert_eq(list_size(master->workers_table->busy_list), 0);
    cr_assert_eq(master->workers_table->total_workers_connected, 1);
    
    destroy_fake_master(master);
}

Test(worker_management, multiple_workers) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    t_worker_control_block *w1 = create_worker(master->workers_table, "1", 100);
    t_worker_control_block *w2 = create_worker(master->workers_table, "2", 101);
    t_worker_control_block *w3 = create_worker(master->workers_table, "3", 102);
    
    cr_assert_not_null(w1);
    cr_assert_not_null(w2);
    cr_assert_not_null(w3);
    
    cr_assert_eq(w1->worker_id, 1);
    cr_assert_eq(w2->worker_id, 2);
    cr_assert_eq(w3->worker_id, 3);
    
    cr_assert_eq(list_size(master->workers_table->worker_list), 3);
    cr_assert_eq(list_size(master->workers_table->idle_list), 3);
    cr_assert_eq(master->workers_table->total_workers_connected, 3);
    
    destroy_fake_master(master);
}

Test(worker_management, worker_initial_state_idle) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    t_worker_control_block *wcb = create_worker(master->workers_table, "42", 200);
    
    cr_assert_eq(wcb->state, WORKER_STATE_IDLE);
    cr_assert_eq(wcb->current_query_id, -1);
    
    // Verificar que está en idle_list, no en busy_list
    cr_assert(list_remove_element(master->workers_table->idle_list, wcb));
    cr_assert_not(list_remove_element(master->workers_table->busy_list, wcb));
    
    destroy_fake_master(master);
}

Test(worker_management, worker_id_parsing) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Probar diferentes formatos de ID
    t_worker_control_block *w1 = create_worker(master->workers_table, "0", 100);
    t_worker_control_block *w2 = create_worker(master->workers_table, "999", 101);
    t_worker_control_block *w3 = create_worker(master->workers_table, "42", 102);
    
    cr_assert_eq(w1->worker_id, 0);
    cr_assert_eq(w2->worker_id, 999);
    cr_assert_eq(w3->worker_id, 42);
    
    destroy_fake_master(master);
}