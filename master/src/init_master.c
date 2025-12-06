#include <commons/log.h>
#include "init_master.h"
#include "query_control_manager.h"
#include "worker_manager.h"
#include "aging.h"
#include <stdlib.h>
#include <string.h>

t_master* init_master(char *ip, char *port, int aging_interval, char *scheduling_algorithm, t_log *logger) {
    t_master *master = malloc(sizeof(t_master));
    if (master == NULL) {
        log_error(logger, "No se pudo asignar memoria para la estructura t_master");
        exit(EXIT_FAILURE);
    }

    // Inicializar tablas
    master->queries_table = malloc(sizeof(t_query_table));
    if (master->queries_table == NULL) {
        log_error(logger, "No se pudo asignar memoria para la tabla de queries");
        goto error;
    }
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
    if (master->workers_table == NULL) {
        log_error(logger, "No se pudo asignar memoria para la tabla de workers");
        goto error;
    }
    master->workers_table->worker_list = list_create();
    master->workers_table->total_workers_connected = 0;
    master->workers_table->idle_list = list_create();
    master->workers_table->busy_list = list_create();
    master->workers_table->disconnected_list = list_create();
    pthread_mutex_init(&master->workers_table->worker_table_mutex, NULL);

    // Inicializar datos de configuración
    master->ip = strdup(ip);
    if (master->ip == NULL) {
        log_error(logger, "No se pudo asignar memoria para la IP del Master");
        goto error;
    }
    master->port = strdup(port);
    if (master->port == NULL) {
        log_error(logger, "No se pudo asignar memoria para el puerto del Master");
        goto error;
    }

    // Planificador
    master->scheduling_algorithm = strdup(scheduling_algorithm);
    if (master->scheduling_algorithm == NULL) {
        log_error(logger, "No se pudo asignar memoria para el algoritmo de planificación");
        goto error;
    }
    // Aging
    master->aging_interval = aging_interval;

    master->multiprogramming_level = 0; // Inicialmente 0, se actualizará con las conexiones de workers

    master->logger = logger;
    log_info(master->logger, "Estructura t_master inicializada correctamente");
    return master;

error:
    if (master != NULL) {
        if (master->ip != NULL) free(master->ip);
        if (master->port != NULL) free(master->port);
        if (master->scheduling_algorithm != NULL) free(master->scheduling_algorithm);
        if (master->workers_table != NULL) free(master->workers_table);
        if (master->queries_table != NULL) free(master->queries_table);
        free(master);
    }
    exit(EXIT_FAILURE);
}

void destroy_master(t_master *master) {
    if (!master) return;
    
    // Guardar antes de liberar el string
    bool is_priority = master->scheduling_algorithm && 
                       (strcmp(master->scheduling_algorithm, "PRIORITY") == 0);
    
    if (master->queries_table) {
        if (master->queries_table->query_list) {
            while (!list_is_empty(master->queries_table->query_list)) {
                t_query_control_block *qcb = list_remove(master->queries_table->query_list, 0);
                if (qcb) {
                    if (qcb->query_file_path) {
                        free(qcb->query_file_path);
                    }
                    free(qcb);
                }
            }
            list_destroy(master->queries_table->query_list);
        }
        
        // Destruir cada lista solo si existe
        if (master->queries_table->ready_queue) {
            list_destroy(master->queries_table->ready_queue);
        }
        if (master->queries_table->running_list) {
            list_destroy(master->queries_table->running_list);
        }
        if (master->queries_table->completed_list) {
            list_destroy(master->queries_table->completed_list);
        }
        if (master->queries_table->canceled_list) {
            list_destroy(master->queries_table->canceled_list);
        }
        
        pthread_mutex_destroy(&master->queries_table->query_table_mutex);
        free(master->queries_table);
    }
    
    if (master->workers_table) {
        if (master->workers_table->worker_list) {
            while (!list_is_empty(master->workers_table->worker_list)) {
                t_worker_control_block *wcb = list_remove(master->workers_table->worker_list, 0);
                if (wcb) {
                    if (wcb->ip_address) {
                        free(wcb->ip_address);
                    }
                    free(wcb);
                }
            }
            list_destroy(master->workers_table->worker_list);
        }
        
        // Destruir cada lista (solo si existe)
        if (master->workers_table->idle_list) {
            list_destroy(master->workers_table->idle_list);
        }
        if (master->workers_table->busy_list) {
            list_destroy(master->workers_table->busy_list);
        }
        if (master->workers_table->disconnected_list) {
            list_destroy(master->workers_table->disconnected_list);
        }
        
        pthread_mutex_destroy(&master->workers_table->worker_table_mutex);
        free(master->workers_table);
    }
    
    // Liberar los demás campos
    if (master->ip) free(master->ip);
    if (master->port) free(master->port);
    if (master->scheduling_algorithm) free(master->scheduling_algorithm);
    
    // join de thread (si estamos en priority)
    if (is_priority) {
        pthread_join(master->aging_thread, NULL);
        if (master->logger) {
            log_info(master->logger, "Aging thread finalizado correctamente.");
        }
    }
    
    if (master->logger) log_destroy(master->logger);
    
    free(master);
}