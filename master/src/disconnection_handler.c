#include "disconnection_handler.h"

#include <commons/collections/list.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "init_master.h"
#include "query_control_manager.h"
#include "worker_manager.h"
#include "connection/serialization.h"

static int running_query_id_for_predicate = -1;
static bool predicate_query_id_equals_running_id(void *elem) {
    t_query_control_block *query = (t_query_control_block*)elem;
    return query && (query->query_id == running_query_id_for_predicate);
}

static int search_worker_id = -1;
static bool match_worker_by_id(void *element) {
    t_worker_control_block *worker = (t_worker_control_block *) element;
    if (worker == NULL) return false;
    return (worker->worker_id == search_worker_id);
}

int handle_query_control_disconnection(int client_socket, t_master *master) {
    if (master == NULL || master->queries_table == NULL) {
        if (master && master->logger) log_error(master->logger, "[handle_query_control_disconnection] master o query_table NULL");
        return -1;
    }

    log_info(master->logger, "[handle_query_control_disconnection] Query Control disconnected on socket %d", client_socket);

    // Buscar QCB asociado al socket del QC
    // Bloqueamos la tabla de queries para acceder segura
    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, "[handle_query_control_disconnection] Error locking query_table_mutex");
        return -1;
    }

    t_query_control_block *qcb = find_query_by_socket(master->queries_table, client_socket);

    if (!qcb) {
        log_warning(master->logger, "[handle_query_control_disconnection] No se encontró QC en socket %d", client_socket);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        close(client_socket);
        return -1;
    }

    // Dependiendo del estado de la query, actuar
    switch (qcb->state) {
        case QUERY_STATE_READY:
            cancel_query_in_ready(qcb, master);
            break;
        case QUERY_STATE_RUNNING:
            // Para notificar al worker, necesitamos el worker_table mutex
            // Siguiendo la convención: lock workers después queries para evitar posibles deadlocks.
            if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
                log_error(master->logger, "[handle_query_control_disconnection] Error locking worker_table_mutex");
                // Se intenta cancelar sin lock (podría fallar si otro thread está modificando), pero hay que terminar la query...
                cancel_query_in_exec(qcb, master);
            } else {
                cancel_query_in_exec(qcb, master);
                pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
            }
            break;
        case QUERY_STATE_NEW:
        case QUERY_STATE_COMPLETED:
        case QUERY_STATE_CANCELED:
        default:
            log_debug(master->logger, "[handle_query_control_disconnection] Query ID=%d en estado %d, se mueve a CANCELED/EXIT",
                      qcb->query_id, qcb->state);
            finalize_query_with_error(qcb, master, "Query cancelada porque Query Control se desconectó");
            break;
    }

    // Cleanup y cierre de socket del QC
    cleanup_query_resources(qcb, master);

    pthread_mutex_unlock(&master->queries_table->query_table_mutex);

    // Cerrar socket del QC (si todavía sigue abierto)
    close(client_socket);

    return 0;
}

int handle_worker_disconnection(int client_socket, t_master *master) {
    if (master == NULL || master->workers_table == NULL) {
        if (master && master->logger) log_error(master->logger, "[handle_worker_disconnection] master o workers_table NULL");
        return -1;
    }

    log_info(master->logger, "[handle_worker_disconnection] Worker disconnected on socket %d", client_socket);
    
    // Bloquear tabla de workers para acceso seguro, manteniendo convención de locking
    if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
        log_error(master->logger, "[handle_worker_disconnection] Error al bloquear worker_table_mutex");
        return -1;
    }

    t_worker_control_block *wcb = find_worker_by_socket(master->workers_table, client_socket);
    if (!wcb) {
        log_warning(master->logger, "[handle_worker_disconnection] No se encontró Worker para socket %d", client_socket);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        close(client_socket);
        return -1;
    }

    // Si tenía una query en ejecución, marcarla como finalizada con error y notificar al QC
    int running_query_id = wcb->current_query_id;
    if (running_query_id >= 0) {
        // Necesitamos consultar la query en la queries_table, por lo tanto bloquearla
        if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
            log_error(master->logger, "[handle_worker_disconnection] Error locking query_table_mutex");
            // En caso de fallo en bloqueo, igual marcamos en worker y continuamos con best-effort
        } else {
            // Buscar la QCB por ID (no por socket en este caso)
            // Recorremos queries_table->query_list y comparamos query_id
            t_query_control_block *qcb = NULL;

            // Predicate for list_find: es true cuando query id es igual a running_query_id en worker.
            running_query_id_for_predicate = running_query_id;
            t_query_control_block *qcb = list_find(master->queries_table->query_list, predicate_query_id_equals_running_id);

            if (qcb) {
                log_warning(master->logger, "[handle_worker_disconnection] Worker socket %d desconectado mientras realizaba la Query ID=%d (QC socket=%d)",
                            client_socket, qcb->query_id, qcb->socket_fd);

                finalize_query_with_error(qcb, master, "Se desconectó worker mientras realizaba la query");
                // mover qcb a canceled/completed/exit según manejo interno
                cleanup_query_resources(qcb, master);
            } else {
                log_warning(master->logger, "[handle_worker_disconnection] Running query ID=%d no encontrada en queries_table", running_query_id);
            }

            pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        }
    }

    // Remover worker de las listas y liberar recursos
    cleanup_worker_resources(wcb, master);

    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    // Cerrar socket
    close(client_socket);

    return 0;
}

void cancel_query_in_ready(t_query_control_block *qcb, t_master *master) {
    if (!qcb || !master) return;

    log_debug(master->logger, "[cancel_query_in_ready] Cancelando Query ID=%d (QC socket=%d) en estado READY", qcb->query_id, qcb->socket_fd);

    // El caller tiene lock sobre queries_table; actualizar estado y notificar
    qcb->state = QUERY_STATE_CANCELED;

    finalize_query_with_error(qcb, master, "Se cancela Query - QC desconectada (READY)");
}

int cancel_query_in_exec(t_query_control_block *qcb, t_master *master) {
    if (!qcb || !master) return -1;

    log_debug(master->logger, "[cancel_query_in_exec] Cancelando Query ID=%d en estado EXEC (QC socket=%d, asignada a worker id=%d)",
             qcb->query_id, qcb->socket_fd, qcb->assigned_worker_id);

    // Buscar worker por assigned_worker_id en workers_table
    if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
        log_error(master->logger, "[cancel_query_in_exec] Error al bloquear worker_table_mutex (best-effort continue)");
        // continue best-effort
    }

    // Encontrar worker by id
    t_worker_control_block *target_worker = NULL;
    if (master->workers_table && master->workers_table->worker_list) {
        search_worker_id = qcb->assigned_worker_id;
        target_worker = list_find(master->workers_table->worker_list, match_worker_by_id);
    }

    if (!target_worker) {
        log_error(master->logger, "[cancel_query_in_exec] Assigned worker id=%d not found", qcb->assigned_worker_id);
        if (pthread_mutex_unlock(&master->workers_table->worker_table_mutex) != 0) {
            log_error(master->logger, "[cancel_query_in_exec] Error unlocking worker_table_mutex");
        }
        // finalizar query con error ya que worker no está disponible
        finalize_query_with_error(qcb, master, "Assigned worker not available to evict query");
        return -1;
    }

    // Si el worker no tiene socket válido -> finalizar con error
    if (target_worker->socket_fd <= 0) {
        log_error(master->logger, "[cancel_query_in_exec] Worker ID=%d has invalid socket (%d)", target_worker->worker_id, target_worker->socket_fd);
        finalize_query_with_error(qcb, master, "Assigned worker socket invalid during cancellation");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

    // Enviar petición de eviction al worker
    t_package *pkg = package_create_empty(OP_WORKER_EVICT_REQ);
    if (!pkg) {
        log_error(master->logger, "[cancel_query_in_exec] Cannot create eviction package");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        finalize_query_with_error(qcb, master, "Internal error: couldn't create eviction package");
        return -1;
    }

    // Agregamos query_id para que el worker sepa qué desalojar
    if (!package_add_uint32(pkg, (uint32_t)qcb->query_id)) {
        log_error(master->logger, "[cancel_query_in_exec] Error adding query_id to eviction package");
        package_destroy(pkg);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        finalize_query_with_error(qcb, master, "Internal error: couldn't serialize eviction package");
        return -1;
    }

    if (package_send(pkg, target_worker->socket_fd) != 0) {
        log_error(master->logger, "[cancel_query_in_exec] Error sending eviction request to Worker ID=%d (socket=%d)",
                  target_worker->worker_id, target_worker->socket_fd);
        package_destroy(pkg);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        finalize_query_with_error(qcb, master, "Failed to send eviction request to worker");
        return -1;
    }

    log_info(master->logger, "[cancel_query_in_exec] Eviction request sent to Worker ID=%d for Query ID=%d",
             target_worker->worker_id, qcb->query_id);

    // Marcar la query como CANCELED
    qcb->state = QUERY_STATE_CANCELED;

    package_destroy(pkg);

    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    // La respuesta del worker deberá ser manejada por handle_eviction_response cuando llegue
    return 0;
}