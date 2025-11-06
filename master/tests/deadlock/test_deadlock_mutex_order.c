#include "unity.h"
#include <unistd.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../../src/init_master.h"
#include "../../src/query_control_manager.h"
#include "../../src/worker_manager.h"
#include <commons/collections/list.h>

#define CANTIDAD_ITERACIONES 10
#define INTERVALO_CREACION_US 200000 // 200ms
#define INTERVALO_REORDENAMIENTO_US 250000 // 250ms

void setUp(void) {}
void tearDown(void) {}
void destroy_fake_master(t_master *master);
t_master* init_fake_master();

volatile bool carga_finalizada = false;
volatile bool reordenamiento_finalizado = false;

bool comparador_prioridad(void* a, void* b) {
    t_query_control_block* q1 = (t_query_control_block*)a;
    t_query_control_block* q2 = (t_query_control_block*)b;
    return q1->priority - q2->priority;
}

void* hilo_carga_queries(void* arg) {
    t_master* master = (t_master*)arg;

    for (int i = 0; i < CANTIDAD_ITERACIONES; i++) {
        t_query_control_block* qcb = malloc(sizeof(t_query_control_block));
        qcb->query_id = i;
        qcb->priority = 18 - (i % 3); // prioridad variable

        pthread_mutex_lock(&master->queries_table->query_table_mutex);
        list_add(master->queries_table->ready_queue, qcb);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);

        usleep(INTERVALO_CREACION_US);
    }

    carga_finalizada = true;
    return NULL;
}

void* hilo_reordenamiento(void* arg) {
    t_master* master = (t_master*)arg;

    for (int i = 0; i < CANTIDAD_ITERACIONES; i++) {
        pthread_mutex_lock(&master->queries_table->query_table_mutex);
        list_sort(master->queries_table->ready_queue, comparador_prioridad);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);

        usleep(INTERVALO_REORDENAMIENTO_US);
    }

    reordenamiento_finalizado = true;
    return NULL;
}

void test_concurrencia_carga_y_reordenamiento() {
    t_master* master = init_fake_master();

    pthread_t carga_thread, reordenamiento_thread;
    pthread_create(&carga_thread, NULL, hilo_carga_queries, master);
    pthread_create(&reordenamiento_thread, NULL, hilo_reordenamiento, master);

    int elapsed = 0;
    while (elapsed < 5000) {
        if (carga_finalizada && reordenamiento_finalizado) break;
        usleep(10000);
        elapsed += 10;
    }

    if (!carga_finalizada || !reordenamiento_finalizado) {
        TEST_FAIL_MESSAGE("⚠️ Posible deadlock: los hilos no finalizaron.");
    } else {
        printf("✅ Cola final con %d elementos\n", list_size(master->queries_table->ready_queue));
        TEST_PASS_MESSAGE("Ambos hilos finalizaron sin deadlock.");
    }

    pthread_join(carga_thread, NULL);
    pthread_join(reordenamiento_thread, NULL);
    destroy_fake_master(master);
}



int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_concurrencia_carga_y_reordenamiento);

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