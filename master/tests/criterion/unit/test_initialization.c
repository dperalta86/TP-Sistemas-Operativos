#include <criterion/criterion.h>
#include "init_master.h"
#include "config/master_config.h"
#include "query_control_manager.h"
#include "worker_manager.h"
#include "scheduler.h"
#include "aging.h"
#include "../../helpers/test_helpers.h"

Test(initialization, create_master_config_valid) {
    t_master_config *config = create_master_config("criterion/unit/test_configs/valid.config");
    cr_assert_not_null(config);
    cr_assert_str_eq(config->ip, "127.0.0.1");
    cr_assert_str_eq(config->port, "9001");
    cr_assert_str_eq(config->scheduler_algorithm, "FIFO");
    cr_assert_eq(config->aging_time, 2500);
    destroy_master_config_instance(config);
}

Test(initialization, create_master_config_invalid_path) {
    t_master_config *config = create_master_config("nonexistent.config");
    cr_assert_null(config);
}

Test(initialization, init_master_structure) {
    t_log *logger = log_create("test.log", "TEST", true, LOG_LEVEL_DEBUG);
    t_master *master = init_fake_master("FIFO", 1000);
    
    cr_assert_not_null(master);
    cr_assert_not_null(master->queries_table);
    cr_assert_not_null(master->workers_table);
    cr_assert_eq(master->queries_table->total_queries, 0);
    cr_assert_eq(master->workers_table->total_workers_connected, 0);
    cr_assert_eq(master->multiprogramming_level, 0);
    
    destroy_master(master);
    log_destroy(logger);
}