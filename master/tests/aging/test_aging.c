#include "unity.h"
#include <unistd.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include "../../src/init_master.h"
#include "../../src/query_control_manager.h"
#include "../../src/scheduler.h"
#include "../../src/aging.h"

void setUp(void) {}
void tearDown(void) {}
t_master* init_fake_master();
void destroy_fake_master(t_master*);

void test_aging_increases_priority_after_interval(void) {
    t_master* fake_master = init_fake_master();
    fake_master->running = true;
        
    if(strcmp(fake_master->scheduling_algorithm, "PRIORITY") == 0) {
        pthread_create(&fake_master->aging_thread, NULL, aging_thread_func, fake_master);
        log_info(fake_master->logger, "Aging habilitado (scheduler PRIORITY)");
    } else {
        log_info(fake_master->logger, "Aging deshabilitado (scheduler FIFO)");
    }

    t_query_control_block* qcb = malloc(sizeof(t_query_control_block));
    qcb->query_id = 1;
    qcb->priority = 3;
    qcb->state = QUERY_STATE_READY;
    qcb->ready_timestamp = now_ms_monotonic();
    list_add(fake_master->queries_table->ready_queue, qcb);

    usleep(600 * 1000); // esperar un poco más que el aging_interval

    // Chequeo esperado: prioridad debe disminuir a 2
    TEST_ASSERT_EQUAL_INT(2, qcb->priority);

    // Finalizar hilo correctamente
    fake_master->running = false;
    pthread_join(fake_master->aging_thread, NULL);

    // Liberar memoria
    destroy_fake_master(fake_master);
/*     pthread_mutex_destroy(&master->queries_table->query_table_mutex);
    list_destroy(master->queries_table->ready_queue);
    log_destroy(master.logger);
    master.logger = NULL;

    free(master.queries_table); */
    free(qcb);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aging_increases_priority_after_interval);
    return UNITY_END();
}

t_master* init_fake_master() {
    t_master *master = malloc(sizeof(t_master));
    if (master == NULL) {
        printf("No se pudo asignar memoria para la estructura t_master");
        exit(EXIT_FAILURE);
    }

    // Inicializo el logger
    master->logger = log_create("test.log", "master_test", true, LOG_LEVEL_INFO);

    // Inicializar tablas
    master->queries_table = malloc(sizeof(t_query_table));
    master->queries_table->query_list = list_create();
    master->queries_table->total_queries = 0;
    master->queries_table->next_query_id = -1; // Comienza en 0, se incrementa con cada nueva query

    // Crear e inicializar listas de estados (utilizando las commons)
    master->queries_table->ready_queue = list_create();
    master->queries_table->running_list = list_create();
    master->queries_table->completed_list = list_create();
    master->queries_table->canceled_list = list_create();
    pthread_mutex_init(&master->queries_table->query_table_mutex, NULL);

    master->workers_table = malloc(sizeof(t_worker_table));
    master->workers_table->worker_list = list_create();
    master->workers_table->total_workers_connected = 0;
    master->workers_table->idle_list = list_create();
    master->workers_table->busy_list = list_create();
    master->workers_table->disconnected_list = list_create();
    pthread_mutex_init(&master->workers_table->worker_table_mutex, NULL);

    // Inicializar datos de configuración
    master->ip = strdup("127.0.0.1");
    master->port = strdup("8080");

    // Planificador
    master->scheduling_algorithm = strdup("PRIORITY");
    master->aging_interval = 500;

    master->multiprogramming_level = 0; // Inicialmente 0, se actualizará con las conexiones de workers

    return master;
}

void destroy_fake_master(t_master *master) {
    // TODO: Verificar si es necesario liberar las listas dentro de las tablas
    if (master != NULL) {
        int priority_algorithm = strcmp(master->scheduling_algorithm, "PRIORITY");
        if (master->ip != NULL) free(master->ip);
        if (master->scheduling_algorithm != NULL) free(master->scheduling_algorithm);
        if (master->workers_table != NULL) {
            free(master->workers_table);
        }
        if (master->queries_table != NULL) {
            free(master->queries_table);
        }
        if(priority_algorithm == 0) {
            pthread_join(master->aging_thread, NULL);
            log_info(master->logger, "Aging thread finalizado correctamente.");
        }
        free(master);
    }
}