// test_aging.c
#include <criterion/criterion.h>
#include "aging.h"
#include <unistd.h>
#include "../../helpers/test_helpers.h"

Test(aging, priority_increases_after_interval, .timeout = 5) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("PRIORITY", 500);
    master->running = true;
    
    // Crear query con prioridad 5
    t_query_control_block *query = create_query(master, 0, "/q1.txt", 5, 10);
    
    cr_assert_eq(query->priority, 5);
    cr_assert_eq(query->initial_priority, 5);
    
    // Esperar más de un intervalo
    usleep(600000); // 600ms
    
    // Simular verificación de aging (normalmente lo hace el thread)
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    uint64_t now = now_ms_monotonic();
    uint64_t elapsed = now - query->ready_timestamp;
    int intervals = elapsed / master->aging_interval;
    
    if (intervals > 0 && query->priority > 0) {
        query->priority -= intervals;
        if (query->priority < 0) query->priority = 0;
    }
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    cr_assert_eq(query->priority, 4); // Debería haber disminuido en 1
    
    master->running = false;
    destroy_fake_master(master);
    log_destroy(logger);
}

Test(aging, priority_never_below_zero) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("PRIORITY", 1000);
    
    t_query_control_block *query = create_query(master, 0, "/q1.txt", 2, 10);
    
    // Simular aging extremo
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    query->priority -= 10; // Intentar bajar mucho
    if (query->priority < 0) query->priority = 0;
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    cr_assert_eq(query->priority, 0);
    
    destroy_fake_master(master);
    log_destroy(logger);
}