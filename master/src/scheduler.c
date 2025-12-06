#include <stdio.h>

#include "scheduler.h"
#include "init_master.h"
#include "worker_manager.h"
#include "query_control_manager.h"

int try_dispatch(t_master *master) {
    if (master == NULL || master->workers_table == NULL || master->queries_table == NULL) {
        log_error(master ? master->logger : NULL, "[try_dispatch] Estructura master inválida o no inicializada.");
        return -1;
    }

    int result = 0;

    // Bloqueo ordenado: primero workers, luego queries
    if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
        log_error(master->logger, "[try_dispatch] Error al bloquear mutex de workers.");
        return -1;
    }

    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, "[try_dispatch] Error al bloquear mutex de queries.");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

    // Validaciones
    if (master->workers_table->idle_list == NULL || master->queries_table->ready_queue == NULL) {
        log_error(master->logger, "[try_dispatch] Listas no inicializadas (idle_list o ready_queue).");
        result = -1;
        goto unlock_and_exit;
    }

    // Si no hay workers o queries, simplemente salimos sin error
    if (list_is_empty(master->workers_table->idle_list)) {
        log_debug(master->logger, "[try_dispatch] No hay workers IDLE disponibles.");
        goto unlock_and_exit;
    }

    if (list_is_empty(master->queries_table->ready_queue)) {
        log_debug(master->logger, "[try_dispatch] No hay queries READY para despachar.");
        goto unlock_and_exit;
    }

    // Verificar si hay preemptions pendientes
    bool has_pending_preemptions = false;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *q = list_get(master->queries_table->running_list, i);
        if (q && q->preemption_pending) {
            has_pending_preemptions = true;
            log_debug(master->logger, 
                      "[try_dispatch] Query ID=%d tiene desalojo pendiente, esperando respuesta",
                      q->query_id);
            break;
        }
    }
    
    if (has_pending_preemptions) {
        log_debug(master->logger, 
                  "[try_dispatch] Hay desalojos pendientes, postponiendo dispatch");
        goto unlock_and_exit;
    }

    // FIFO: tomar el primero de cada lista
    t_query_control_block *query = list_remove(master->queries_table->ready_queue, 0);
    t_worker_control_block *worker = list_remove(master->workers_table->idle_list, 0);

    if (query == NULL || worker == NULL) {
        log_error(master->logger, "[try_dispatch] Error inesperado: query o worker NULL al remover de las listas.");
        result = -1;
        goto unlock_and_exit;
    }

    // Actualizar estados
    worker->current_query_id = query->query_id;
    worker->state = WORKER_STATE_BUSY;
    query->state = QUERY_STATE_RUNNING;
    query->assigned_worker_id = worker->worker_id;
    query->preemption_pending = false; 

    // Mover a las listas activas
    list_add(master->queries_table->running_list, query);
    list_add(master->workers_table->busy_list, worker);

    log_debug(master->logger,
        "[try_dispatch] Asignada Query ID=%d al Worker ID=%d",
        query->query_id, worker->worker_id);

    // Enviar mensaje al worker para iniciar la ejecución de la query
    if (send_query_to_worker(master, worker, query) != 0) {
        log_error(master->logger, "[try_dispatch] Error al enviar inicio de query al Worker ID=%d para Query ID=%d",
                  worker->worker_id, query->query_id);

        // Revertir cambios en caso de error
        worker->current_query_id = -1;
        worker->state = WORKER_STATE_IDLE;
        query->state = QUERY_STATE_READY;
        query->assigned_worker_id = -1;

        if (!list_remove_element(master->queries_table->running_list, query)) {
            log_warning(master->logger, "[try_dispatch] Query ID=%d no estaba en running_list al revertir.",
                        query->query_id);
        }

        if (!list_remove_element(master->workers_table->busy_list, worker)) {
            log_warning(master->logger, "[try_dispatch] Worker ID=%d no estaba en busy_list al revertir.",
                        worker->worker_id);
        }

        // Volver a colocarlos en sus listas originales
        if (strcmp(master->scheduling_algorithm, "PRIORITY") == 0) {
            insert_query_by_priority(master->queries_table->ready_queue, query);
        } else {
            list_add(master->queries_table->ready_queue, query);
        }

        list_add(master->workers_table->idle_list, worker);

        log_debug(master->logger, "[try_dispatch] Revertidos cambios tras error en envío de query.");

        result = -1;
        goto unlock_and_exit;
    }

unlock_and_exit:
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
    return result;
}

int send_query_to_worker(t_master *master, t_worker_control_block *worker, t_query_control_block *query) {
    #ifdef TEST_ENVIRONMENT
    log_info(master->logger, "[send_query_to_worker] (TEST) Simulación de envío de Query ID=%d al Worker ID=%d", query->query_id, worker->worker_id);
    return 0;
    #endif
    
    // Validaciones básicas
    if (master == NULL || worker == NULL || query == NULL) {
        fprintf(stderr, "[send_query_to_worker] Parámetros inválidos (master, worker o query NULL).\n");
        if (master && master->logger)
            log_error(master->logger, "[send_query_to_worker] Parámetros inválidos (alguno es NULL).");
        return -1;
    }

    if (worker->socket_fd <= 0) {
        log_error(master->logger, "[send_query_to_worker] Worker ID=%d tiene un socket inválido (%d).",
                  worker->worker_id, worker->socket_fd);
        return -1;
    }

    // Crear paquete
    t_package *package_send_query = package_create_empty(OP_WORKER_START_QUERY);
    if (package_send_query == NULL) {
        log_error(master->logger, "[send_query_to_worker] Error al crear paquete para Worker ID=%d.",
                  worker->worker_id);
        return -1;
    }

    // Agrego query ID, PC y path
    if (package_add_uint32(package_send_query, query->query_id) != true) {
        log_error(master->logger, "[send_query_to_worker] Error al agregar id (%d) de Query al paquete para Worker ID=%d.",
                  query->query_id, worker->worker_id);
        package_destroy(package_send_query);
        return -1;
    }

    if (package_add_uint32(package_send_query, query->program_counter) != true) {
        log_error(master->logger, "[send_query_to_worker] Error al agregar path de Query ID=%d al paquete para Worker ID=%d.",
                  query->query_id, worker->worker_id);
        package_destroy(package_send_query);
        return -1;
    }

    if (package_add_string(package_send_query, query->query_file_path) != true) {
        log_error(master->logger, "[send_query_to_worker] Error al agregar path de Query ID=%d al paquete para Worker ID=%d.",
                  query->query_id, worker->worker_id);
        package_destroy(package_send_query);
        return -1;
    }

    // Enviar paquete al worker
    if (package_send(package_send_query, worker->socket_fd) != 0) {
        log_error(master->logger, "[send_query_to_worker] Error al enviar paquete de Query ID=%d al Worker ID=%d.",
                  query->query_id, worker->worker_id);
        package_destroy(package_send_query);
        return -1;
    }

    // Log de éxito
    log_info(master->logger, "## Se envía la Query id: %d (prioridad: %d) al Worker id: %d",
             query->query_id, query->priority, worker->worker_id);

    // Liberar memoria
    package_destroy(package_send_query);
    return 0;
}
