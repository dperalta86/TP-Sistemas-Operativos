#include <criterion/criterion.h>
#include "../../../src/init_master.h"
#include "../../../src/query_control_manager.h"
#include "../../helpers/test_helpers.h"

// ==========================================
// Tests de Gestión de Queries
// ==========================================

Test(query_management, create_query_basic) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    t_query_control_block *qcb = create_query(
        master, 
        0,                    // query_id
        "/path/to/query.qry", // path
        5,                    // priority
        100                   // socket_fd
    );
    
    cr_assert_not_null(qcb);
    cr_assert_eq(qcb->query_id, 0);
    cr_assert_str_eq(qcb->query_file_path, "/path/to/query.qry");
    cr_assert_eq(qcb->priority, 5);
    cr_assert_eq(qcb->initial_priority, 5);
    cr_assert_eq(qcb->socket_fd, 100);
    cr_assert_eq(qcb->state, QUERY_STATE_READY);
    cr_assert_eq(qcb->assigned_worker_id, -1);
    cr_assert_eq(qcb->program_counter, 0);
    
    // Verificar que está en las listas correctas
    cr_assert_eq(list_size(master->queries_table->query_list), 1);
    cr_assert_eq(list_size(master->queries_table->ready_queue), 1);
    cr_assert_eq(master->queries_table->total_queries, 1);
    
    destroy_fake_master(master);
}

Test(query_management, multiple_queries_fifo) {
    t_master *master = init_fake_master("FIFO", 1000);
    
    // Crear 3 queries
    t_query_control_block *q1 = create_query(master, 0, "/q1.qry", 5, 100);
    t_query_control_block *q2 = create_query(master, 1, "/q2.qry", 3, 101);
    t_query_control_block *q3 = create_query(master, 2, "/q3.qry", 8, 102);
    
    cr_assert_not_null(q1);
    cr_assert_not_null(q2);
    cr_assert_not_null(q3);
    
    // En FIFO, el orden en ready_queue es por llegada
    cr_assert_eq(list_size(master->queries_table->ready_queue), 3);
    
    // Verificar orden FIFO (primera en llegar, primera en la cola)
    t_query_control_block *first = list_get(master->queries_table->ready_queue, 0);
    t_query_control_block *second = list_get(master->queries_table->ready_queue, 1);
    t_query_control_block *third = list_get(master->queries_table->ready_queue, 2);
    
    cr_assert_eq(first->query_id, 0);
    cr_assert_eq(second->query_id, 1);
    cr_assert_eq(third->query_id, 2);
    
    destroy_fake_master(master);
}

Test(query_management, multiple_queries_priority_order) {
    t_master *master = init_fake_master("PRIORITY", 1000);
    
    // Crear queries con diferentes prioridades (menor número = mayor prioridad)
    create_query(master, 0, "/q1.qry", 5, 100);  // Prioridad media
    create_query(master, 1, "/q2.qry", 2, 101);  // Prioridad alta
    create_query(master, 2, "/q3.qry", 8, 102);  // Prioridad baja
    create_query(master, 3, "/q4.qry", 2, 103);  // Prioridad alta (igual a q2)
    
    cr_assert_eq(list_size(master->queries_table->ready_queue), 4);
    
    // Verificar orden: q2(2), q4(2), q1(5), q3(8)
    t_query_control_block *first = list_get(master->queries_table->ready_queue, 0);
    t_query_control_block *second = list_get(master->queries_table->ready_queue, 1);
    t_query_control_block *third = list_get(master->queries_table->ready_queue, 2);
    t_query_control_block *fourth = list_get(master->queries_table->ready_queue, 3);
    
    cr_assert_eq(first->priority, 2);
    cr_assert_eq(second->priority, 2);
    cr_assert_eq(third->priority, 5);
    cr_assert_eq(fourth->priority, 8);
    
    destroy_fake_master(master);
}

Test(query_management, insert_query_by_priority_correct_position) {
    t_master *master = init_fake_master("PRIORITY", 1000);
    t_list *queue = master->queries_table->ready_queue;
    
    // Crear queries manualmente y insertar con la función
    t_query_control_block *q1 = malloc(sizeof(t_query_control_block));
    q1->query_id = 0;
    q1->priority = 5;
    
    t_query_control_block *q2 = malloc(sizeof(t_query_control_block));
    q2->query_id = 1;
    q2->priority = 2;
    
    t_query_control_block *q3 = malloc(sizeof(t_query_control_block));
    q3->query_id = 2;
    q3->priority = 8;
    
    // Insertar en orden: media, alta, baja
    cr_assert_eq(insert_query_by_priority(queue, q1), 0);
    cr_assert_eq(insert_query_by_priority(queue, q2), 0);
    cr_assert_eq(insert_query_by_priority(queue, q3), 0);
    
    // Verificar orden final: q2(2), q1(5), q3(8)
    cr_assert_eq(list_size(queue), 3);
    
    t_query_control_block *first = list_get(queue, 0);
    t_query_control_block *second = list_get(queue, 1);
    t_query_control_block *third = list_get(queue, 2);
    
    cr_assert_eq(first->query_id, 1);  // q2
    cr_assert_eq(second->query_id, 0); // q1
    cr_assert_eq(third->query_id, 2);  // q3
    
    destroy_fake_master(master);
}

Test(query_management, query_ready_timestamp_initialized) {
    t_master *master = init_fake_master("PRIORITY", 500);
    
    uint64_t before = now_ms_monotonic();
    t_query_control_block *qcb = create_query(master, 0, "/q1.qry", 5, 100);
    uint64_t after = now_ms_monotonic();
    
    cr_assert_not_null(qcb);
    cr_assert_geq(qcb->ready_timestamp, before);
    cr_assert_leq(qcb->ready_timestamp, after);
    
    destroy_fake_master(master);
}

Test(query_management, insert_query_null_handling) {
    t_master *master = init_fake_master("PRIORITY", 1000);
    
    // Intentar insertar query NULL
    int result = insert_query_by_priority(master->queries_table->ready_queue, NULL);
    cr_assert_eq(result, -1, "Should return error for NULL query");
    
    // Intentar insertar en lista NULL
    t_query_control_block *qcb = malloc(sizeof(t_query_control_block));
    qcb->priority = 5;
    result = insert_query_by_priority(NULL, qcb);
    cr_assert_eq(result, -1, "Should return error for NULL list");
    
    free(qcb);
    destroy_fake_master(master);
}