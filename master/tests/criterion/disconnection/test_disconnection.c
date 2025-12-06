// test_disconnection.c
#include <criterion/criterion.h>
#include "disconnection_handler.h"
#include "../../helpers/test_helpers.h"

Test(disconnection, cancel_query_in_ready) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 1000);
    
    t_query_control_block *query = create_query(master, 0, "/q1.txt", 5, 10);
    
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    cancel_query_in_ready(query, master);
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    cr_assert_eq(query->state, QUERY_STATE_CANCELED);
    
    destroy_fake_master(master);
    log_destroy(logger);
}

Test(disconnection, find_query_by_socket) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 1000);
    
    t_query_control_block *q1 = create_query(master, 0, "/q1.txt", 5, 100);
    t_query_control_block *q2 = create_query(master, 1, "/q2.txt", 3, 200);
    
    t_query_control_block *found = find_query_by_socket(master->queries_table, 200);
    
    cr_assert_not_null(found);
    cr_assert_eq(found->query_id, 1);
    cr_assert_eq(found->socket_fd, 200);
    
    destroy_fake_master(master);
    log_destroy(logger);
}

Test(disconnection, find_worker_by_socket) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 2500);
    
    t_worker_control_block *w1 = create_worker(master->workers_table, "1", 100);
    t_worker_control_block *w2 = create_worker(master->workers_table, "2", 200);
    
    t_worker_control_block *found = find_worker_by_socket(master->workers_table, 200);
    
    cr_assert_not_null(found);
    cr_assert_eq(found->worker_id, 2);
    cr_assert_eq(found->socket_fd, 200);
    
    destroy_fake_master(master);
    log_destroy(logger);
}