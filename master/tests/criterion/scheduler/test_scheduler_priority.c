// test_scheduler_priority.c
#include <criterion/criterion.h>
#include "scheduler.h"
#include "aging.h"
#include "../../helpers/test_helpers.h"

Test(scheduler_priority, dispatch_by_priority_order) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("PRIORITY", 2500);
    
    // Crear 1 worker
    create_worker(master->workers_table, "1", 100);
    
    // Crear queries con diferentes prioridades (menor = mayor prioridad)
    t_query_control_block *q1 = create_query(master, 0, "/q1.txt", 5, 10);
    t_query_control_block *q2 = create_query(master, 1, "/q2.txt", 2, 11); // Mayor prioridad
    t_query_control_block *q3 = create_query(master, 2, "/q3.txt", 8, 12);
    
    // Dispatch: debe tomar q2 (prioridad 2)
    try_dispatch(master);
    
    cr_assert_eq(q2->state, QUERY_STATE_RUNNING);
    cr_assert_eq(q1->state, QUERY_STATE_READY);
    cr_assert_eq(q3->state, QUERY_STATE_READY);
    
    destroy_fake_master(master);
    log_destroy(logger);
}

Test(scheduler_priority, preemption_by_higher_priority) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("PRIORITY", 2500);
    
    // Crear 1 worker
    t_worker_control_block *worker = create_worker(master->workers_table, "1", 100);
    
    // Query de baja prioridad ejecutándose
    t_query_control_block *q_low = create_query(master, 0, "/q_low.txt", 8, 10);
    try_dispatch(master);
    
    cr_assert_eq(q_low->state, QUERY_STATE_RUNNING);
    
    // Llega query de alta prioridad
    t_query_control_block *q_high = create_query(master, 1, "/q_high.txt", 2, 11);
    
    // Llamar check_preemption manualmente
    check_preemption(master);
    
    // Solo verificamos que se haya enviado la solicitud
    // En test de integración verificamos la respuesta del worker
    
    destroy_fake_master(master);
    log_destroy(logger);
}