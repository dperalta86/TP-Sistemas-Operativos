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

// --- Query por ID ---
static int running_query_id_for_predicate = -1;

static bool match_query_by_id(void *elem) {
    t_query_control_block *query = (t_query_control_block*)elem;
    return query && (query->query_id == running_query_id_for_predicate);
}

// --- Worker por ID ---
static int search_worker_id = -1;

static bool match_worker_by_id(void *element) {
    t_worker_control_block *worker = (t_worker_control_block *) element;
    if (worker == NULL) return false;
    return (worker->worker_id == search_worker_id);
}

// --- Query por socket_fd ---
static int search_query_socket_fd = -1;

static bool match_query_by_socket(void *element) {
    t_query_control_block *query = (t_query_control_block *)element;
    if (!query) return false;
    return (query->socket_fd == search_query_socket_fd);
}

// --- Worker por socket_fd ---
static int search_worker_socket_fd = -1;

static bool match_worker_by_socket(void *element) {
    t_worker_control_block *worker = (t_worker_control_block *)element;
    if (!worker) return false;
    return (worker->socket_fd == search_worker_socket_fd);
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
            // Predicate for list_find: es true cuando query id es igual a running_query_id en worker.
            running_query_id_for_predicate = running_query_id;
            t_query_control_block *qcb = list_find(master->queries_table->query_list, match_query_by_id);

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
        log_error(master->logger, "[cancel_query_in_exec] Error al bloquear worker_table_mutex (continuando best-effort)");
        // continue best-effort
    }

    // Encontrar worker by id
    t_worker_control_block *target_worker = NULL;
    if (master->workers_table && master->workers_table->worker_list) {
        search_worker_id = qcb->assigned_worker_id;
        target_worker = list_find(master->workers_table->worker_list, match_worker_by_id);
    }

    if (!target_worker) {
        log_error(master->logger, "[cancel_query_in_exec] No se encontró el worker asignado con id=%d", qcb->assigned_worker_id);
        if (pthread_mutex_unlock(&master->workers_table->worker_table_mutex) != 0) {
            log_error(master->logger, "[cancel_query_in_exec] Error al desbloquear worker_table_mutex");
        }
        // finalizar query con error ya que worker no está disponible
        finalize_query_with_error(qcb, master, "Worker asignado no disponible para desalojar query");
        return -1;
    }

    // Si el worker no tiene socket válido -> finalizar con error
    if (target_worker->socket_fd <= 0) {
        log_error(master->logger, "[cancel_query_in_exec] Worker ID=%d tiene socket inválido (%d)", target_worker->worker_id, target_worker->socket_fd);
        finalize_query_with_error(qcb, master, "Socket del worker inválido durante cancelación");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

    // Enviar petición de eviction al worker
    t_package *pkg = package_create_empty(OP_WORKER_EVICT_REQ);
    if (!pkg) {
        log_error(master->logger, "[cancel_query_in_exec] No se pudo crear paquete de desalojo");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        finalize_query_with_error(qcb, master, "Error interno: no se pudo crear paquete de desalojo");
        return -1;
    }

    // Agregamos query_id para que el worker sepa qué desalojar
    if (!package_add_uint32(pkg, (uint32_t)qcb->query_id)) {
        log_error(master->logger, "[cancel_query_in_exec] Error al agregar query_id al paquete de desalojo");
        package_destroy(pkg);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        finalize_query_with_error(qcb, master, "Error interno: no se pudo serializar paquete de desalojo");
        return -1;
    }

    if (package_send(pkg, target_worker->socket_fd) != 0) {
        log_error(master->logger, "[cancel_query_in_exec] Error al enviar solicitud de desalojo al Worker ID=%d (socket=%d)",
                  target_worker->worker_id, target_worker->socket_fd);
        package_destroy(pkg);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        finalize_query_with_error(qcb, master, "Falló el envío de solicitud de desalojo al worker");
        return -1;
    }

    log_info(master->logger, "[cancel_query_in_exec] Solicitud de desalojo enviada al Worker ID=%d para Query ID=%d",
             target_worker->worker_id, qcb->query_id);

    // Marcar la query como CANCELED
    qcb->state = QUERY_STATE_CANCELED;

    package_destroy(pkg);

    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    // La respuesta del worker deberá ser manejada por handle_eviction_response cuando llegue
    return 0;
}


/**
 * handle_eviction_response:
 *  - worker_socket: socket del worker que responde
 *  - buffer: buffer recibido con la respuesta (estructura propia del paquete)
 *  - master: contexto
 *
 * Se espera que el worker devuelva al menos el query_id y el pc/contexto (opcional).
 * Aquí hacemos un best-effort: leemos query_id y movemos la query a EXIT y notificamos al QC.
 */
int handle_eviction_response(int worker_socket, t_buffer *buffer, t_master *master) {
    if (!master || !master->queries_table) return -1;

    log_info(master->logger, "[handle_eviction_response] Respuesta de desalojo recibida desde worker socket %d", worker_socket);

    // Interpretamos el buffer (asumimos que el primer uint32 es query_id, luego opcionalmente program_counter)
    buffer_reset_offset(buffer);
    uint32_t qid, pc;

    if (buffer_read_uint32(buffer, &qid)) {
        log_warning(master->logger, "[handle_eviction_response] No se pudo leer query_id de la respuesta de desalojo. Búsqueda best-effort por socket del worker.");
        // Si no podemos leer qid, buscaremos por worker->current_query_id
        // Buscar worker por socket
        if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
            log_error(master->logger, "[handle_eviction_response] Error al bloquear worker_table_mutex");
            return -1;
        }
        t_worker_control_block *wcb = find_worker_by_socket(master->workers_table, worker_socket);
        if (!wcb) {
            log_error(master->logger, "[handle_eviction_response] No se encontró worker para el socket %d", worker_socket);
            pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
            return -1;
        }
        qid = wcb->current_query_id;
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        if (qid <= 0) {
            log_error(master->logger, "[handle_eviction_response] No se pudo determinar el id de query para la respuesta de desalojo desde socket %d", worker_socket);
            return -1;
        }
    } else {
        // opcional: leer program_counter si existe
        if (buffer_read_uint32(buffer, &pc)) {
            pc = -1; // si no existe, lo seteamos en -1 (error)
        }
    }

    // Con qid determinado, buscar QCB y notificar QC
    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, "[handle_eviction_response] Error al bloquear query_table_mutex");
        return -1;
    }

    t_query_control_block *qcb = list_find(master->queries_table->query_list, match_query_by_id);

    if (!qcb) {
        log_error(master->logger, "[handle_eviction_response] Query ID=%u no encontrada en la tabla de queries", qid);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        return -1;
    }

    // Actualizamos datos del qcb si recibimos pc u otro contexto (best-effort)
    qcb->program_counter = pc;
    qcb->state = QUERY_STATE_CANCELED;

    // Notificar al Query Control que la query terminó por desalojo (o fue cancellada)
    finalize_query_with_error(qcb, master, "Query evicted by master (worker responded)");

    // Limpiar recursos asociados
    cleanup_query_resources(qcb, master);

    pthread_mutex_unlock(&master->queries_table->query_table_mutex);

    return 0;
}

void finalize_query_with_error(t_query_control_block *qcb, t_master *master, const char *error_reason) {
    if (!qcb || !master) return;

    log_error(master->logger, "[finalize_query_with_error] Finalizando Query ID=%d con error: %s", qcb->query_id, error_reason ? error_reason : "Razón desconocida");

    // Preparar paquete para notificar QC: uso de opcode QC_OP_MASTER_FIN_DESCONEXION
    t_package *pkg = package_create_empty(QC_OP_MASTER_FIN_DESCONEXION);
    if (pkg) {
        // Agregamos query_id y mensaje de error
        package_add_uint32(pkg, (uint32_t)qcb->query_id);
        if (error_reason) package_add_string(pkg, error_reason);

        if (package_send(pkg, qcb->socket_fd) != 0) {
            log_error(master->logger, "[finalize_query_with_error] Error al enviar mensaje de finalización al QC socket %d para Query ID=%d", qcb->socket_fd, qcb->query_id);
        } else {
            log_info(master->logger, "[finalize_query_with_error] Notificación de finalización enviada al QC socket %d para Query ID=%d", qcb->socket_fd, qcb->query_id);
        }
        package_destroy(pkg);
    } else {
        log_error(master->logger, "[finalize_query_with_error] No se pudo crear paquete para notificar al QC para Query ID=%d", qcb->query_id);
    }

    qcb->state = QUERY_STATE_CANCELED;
}

/**
 * cleanup_query_resources
 *  - Remueve la query de las estructuras internas (lists) y libera memoria asociada
 *  - el caller posee el lock correspondiente
 */
void cleanup_query_resources(t_query_control_block *qcb, t_master *master) {
    if (!qcb || !master) return;

    log_debug(master->logger, "[cleanup_query_resources] Limpiando recursos para Query ID=%d", qcb->query_id);

    // Remover de listas si están presentes (best-effort)
    if (master->queries_table && master->queries_table->ready_queue) {
        if (list_remove_element(master->queries_table->ready_queue, qcb)) {
            log_debug(master->logger, "[cleanup_query_resources] Query ID=%d removida de ready_queue", qcb->query_id);
        }
    }
    if (master->queries_table && master->queries_table->running_list) {
        if (list_remove_element(master->queries_table->running_list, qcb)) {
            log_debug(master->logger, "[cleanup_query_resources] Query ID=%d removida de running_list", qcb->query_id);
        }
    }
    if (master->queries_table && master->queries_table->completed_list) {
        if (list_remove_element(master->queries_table->completed_list, qcb)) {
            log_debug(master->logger, "[cleanup_query_resources] Query ID=%d removida de completed_list", qcb->query_id);
        }
    }
    if (master->queries_table && master->queries_table->canceled_list) {
        if (!list_find(master->queries_table->canceled_list, (void*) qcb)) {
            list_add(master->queries_table->canceled_list, qcb);
        }
    }

    // Liberar campos dinámicos
    if (qcb->query_file_path) {
        free(qcb->query_file_path);
        qcb->query_file_path = NULL;
    }
}

/**
 * cleanup_worker_resources
 *  - Remueve el worker de las listas y libera memoria asociada
 *  - Asume que caller posee el lock workers_table->worker_table_mutex
 */
void cleanup_worker_resources(t_worker_control_block *wcb, t_master *master) {
    if (!wcb || !master) return;

    log_debug(master->logger, "[cleanup_worker_resources] Limpiando recursos para Worker ID=%d (socket=%d)", wcb->worker_id, wcb->socket_fd);


    // Remover de listas
    if (master->workers_table && master->workers_table->idle_list) {
        list_remove_element(master->workers_table->idle_list, wcb);
    }
    if (master->workers_table && master->workers_table->busy_list) {
        list_remove_element(master->workers_table->busy_list, wcb);
    }
    if (master->workers_table && master->workers_table->disconnected_list) {
        if (!list_find(master->workers_table->disconnected_list, (void*) wcb)) {
            list_add(master->workers_table->disconnected_list, wcb);
        }
    }

    // Reset fields
    wcb->current_query_id = -1;
    wcb->socket_fd = -1;
    wcb->state = WORKER_STATE_DISCONNECTED;

    // Liberar memoria de strings si existen
    if (wcb->ip_address) {
        free(wcb->ip_address);
        wcb->ip_address = NULL;
    }
}

/**
 * find_query_by_socket
 */
t_query_control_block *find_query_by_socket(t_query_table *table, int socket_fd) {
    if (!table || !table->query_list)
        return NULL;

    search_query_socket_fd = socket_fd;
    return (t_query_control_block *)list_find(table->query_list, match_query_by_socket);
}

/**
 * find_worker_by_socket
 */
t_worker_control_block *find_worker_by_socket(t_worker_table *table, int socket_fd) {
    if (!table || !table->worker_list)
        return NULL;

    search_worker_socket_fd = socket_fd;
    return (t_worker_control_block *)list_find(table->worker_list, match_worker_by_socket);
}
