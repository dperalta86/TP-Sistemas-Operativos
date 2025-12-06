#include "unity.h"
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../../src/init_master.h"
#include "../../src/query_control_manager.h"
#include "../../src/worker_manager.h"
#include "../../src/scheduler.h"
#include <commons/collections/list.h>

#define CANT_QUERIES 10
#define CANT_WORKERS 2
#define TIEMPO_TRABAJO_US 300000 // 300ms
#define TIEMPO_DESPACHO_US 100000 // 100ms

void setUp(void) {}
void tearDown(void) {}
t_master* init_fake_master();
void destroy_fake_master(t_master*);

volatile int queries_finalizadas = 0;
volatile bool test_finalizado = false;

typedef struct {
    t_master* master;
    int worker_index;
} t_worker_thread_data;

void* hilo_worker_simulado(void* arg) {
    t_worker_thread_data* data = (t_worker_thread_data*)arg;
    t_master* master = data->master;
    int index = data->worker_index;

    if (!master || !master->workers_table || !master->workers_table->worker_list) {
        fprintf(stderr, "[ERROR] master o worker_list no inicializados\n");
        free(data);
        pthread_exit(NULL);
    }

    if (index < 0 || index >= list_size(master->workers_table->worker_list)) {
        fprintf(stderr, "[ERROR] Índice de worker fuera de rango: %d\n", index);
        free(data);
        pthread_exit(NULL);
    }

    t_worker_control_block* worker = list_get(master->workers_table->worker_list, index);

    while (!test_finalizado) {
        if (worker->current_query_id >= 0) {
            usleep(TIEMPO_TRABAJO_US);

            pthread_mutex_lock(&master->queries_table->query_table_mutex);
            t_query_control_block* query = NULL;
            for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
                t_query_control_block* qcb = list_get(master->queries_table->running_list, i);
                if (qcb->query_id == worker->current_query_id) {
                    query = qcb;
                    break;
                }
            }

            if (query) {
                query->state = QUERY_STATE_COMPLETED;
                list_remove_element(master->queries_table->running_list, query);
                list_add(master->queries_table->completed_list, query);
                queries_finalizadas++;
            }

            pthread_mutex_unlock(&master->queries_table->query_table_mutex);

            pthread_mutex_lock(&master->workers_table->worker_table_mutex);
            worker->current_query_id = -1;
            worker->state = WORKER_STATE_IDLE;
            list_remove_element(master->workers_table->busy_list, worker);
            list_add(master->workers_table->idle_list, worker);
            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        }

        usleep(50000);
    }

    free(data);
    return NULL;
}


void* hilo_despachador(void* arg) {
    t_master* master = (t_master*)arg;

    while (queries_finalizadas < CANT_QUERIES) {
        try_dispatch(master);
        usleep(TIEMPO_DESPACHO_US);
    }

    test_finalizado = true;
    return NULL;
}

void test_ejecucion_real_sin_deadlock() {
    t_master* master = init_fake_master();

    // Crear hilos
    pthread_t despachador_thread;
    pthread_t worker_threads[CANT_WORKERS];

    // Crear queries
    for (int i = 0; i < CANT_QUERIES; i++) {
        char* path = strdup("dummy/path.qry");
        create_query(master->queries_table, i, path, 5 - (i % 3), -1);
        free(path);
    }

    pthread_create(&despachador_thread, NULL, hilo_despachador, master);
    
    // Crear workers (ANTES de lanzar los hilos)
    for (int i = 0; i < CANT_WORKERS; i++) {
        char id_str[10];
        sprintf(id_str, "%d", i);
        create_worker(master->workers_table, id_str, i + 100); // socket simulado
    }

    // Confirmar que están en la lista
    printf("[DEBUG] worker_list tiene %d elementos\n", list_size(master->workers_table->worker_list));

    // Crear hilos de workers
    for (int i = 0; i < CANT_WORKERS; i++) {
        t_worker_thread_data* data = malloc(sizeof(t_worker_thread_data));
        data->master = master;
        data->worker_index = i;
        pthread_create(&worker_threads[i], NULL, hilo_worker_simulado, data);
    }



    // Esperar finalización
    pthread_join(despachador_thread, NULL);
    for (int i = 0; i < CANT_WORKERS; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(CANT_QUERIES, list_size(master->queries_table->completed_list),
                                  "No todas las queries fueron completadas.");
    TEST_PASS_MESSAGE("✅ Todas las queries fueron ejecutadas y finalizadas sin deadlock.");

    destroy_fake_master(master);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ejecucion_real_sin_deadlock);
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