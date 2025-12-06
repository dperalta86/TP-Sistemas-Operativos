#include "disconnection_handler.h"
#include "scheduler.h"
#include "aging.h"

#include <commons/collections/list.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "init_master.h"
#include "query_control_manager.h"
#include "worker_manager.h"
#include "connection/serialization.h"

// Variables estáticas para predicados de búsqueda
static int running_query_id_for_predicate = -1;
static int search_worker_id = -1;
static int search_query_socket_fd = -1;
static int search_worker_socket_fd = -1;

// --- Query por ID ---
static bool match_query_by_id(void *elem) {
    t_query_control_block *query = (t_query_control_block*)elem;
    return query && (query->query_id == running_query_id_for_predicate);
}

// --- Query por socket_fd ---
static bool match_query_by_socket(void *element) {
    t_query_control_block *query = (t_query_control_block *)element;
    if (!query) return false;
    return (query->socket_fd == search_query_socket_fd);
}

// --- Worker por socket_fd ---
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

    log_debug(master->logger, "[handle_query_control_disconnection] Query Control disconnected on socket %d", client_socket);

    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, "[handle_query_control_disconnection] Error locking query_table_mutex");
        return -1;
    }

    t_query_control_block *qcb = find_query_by_socket(master->queries_table, client_socket);

    if (!qcb) {
        log_debug(master->logger, "[handle_query_control_disconnection] No se encontró QC en socket %d", client_socket);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        close(client_socket);
        return -1;
    }

    if (qcb->cleaned_up) {
        log_debug(master->logger,
                  "[handle_query_control_disconnection] Query ID=%d ya fue limpiada, ignorando desconexión",
                  qcb->query_id);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        close(client_socket);
        return 0;
    }

    int query_id = qcb->query_id;
    t_query_state current_state = qcb->state;
    
    switch (current_state) {
        case QUERY_STATE_READY:
            cancel_query_in_ready(qcb, master);
            cleanup_query_resources(qcb, master);
            break;
            
        case QUERY_STATE_RUNNING:
            if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
                log_error(master->logger, "[handle_query_control_disconnection] Error locking worker_table_mutex");
            } else {
                // Marcar como cancelada ANTES de preempt
                qcb->state = QUERY_STATE_CANCELED;
                
                // preempt_query_in_exec ya setea preemption_pending
                preempt_query_in_exec(qcb, master);
                
                pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
            }
            break;
            
        case QUERY_STATE_COMPLETED:
        case QUERY_STATE_CANCELED:
            log_debug(master->logger, "[handle_query_control_disconnection] Query ID=%d en estado %d",
                      query_id, current_state);
            cleanup_query_resources(qcb, master);
            break;
            
        default:
            finalize_query_with_error(qcb, master, "Query cancelada porque Query Control se desconectó");
            cleanup_query_resources(qcb, master);
            break;
    }

    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    close(client_socket);

    return 0;
}

int handle_worker_disconnection(int client_socket, t_master *master) {
    if (master == NULL || master->workers_table == NULL) {
        if (master && master->logger) log_error(master->logger, "[handle_worker_disconnection] master o workers_table NULL");
        return -1;
    }

    log_debug(master->logger, "[handle_worker_disconnection] Worker disconnected on socket %d", client_socket);
    
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
                log_warning(master->logger, "[handle_worker_disconnection] Worker id %d desconectado mientras realizaba la Query ID=%d (QC socket=%d)",
                            wcb->worker_id, qcb->query_id, qcb->socket_fd);

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

    // Cerrar socket
    close(client_socket);

    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
    return 0;
}

void cancel_query_in_ready(t_query_control_block *qcb, t_master *master) {
    if (!qcb || !master) return;

    log_debug(master->logger, "[cancel_query_in_ready] Cancelando Query ID=%d (QC socket=%d) en estado READY", qcb->query_id, qcb->socket_fd);

    // El caller tiene lock sobre queries_table; actualizar estado y notificar
    qcb->state = QUERY_STATE_CANCELED;

    finalize_query_with_error(qcb, master, "Se cancela Query - QC desconectada (READY)");
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

    // Verificar si ya fue limpiada (para evitar doble free)
    if (qcb->cleaned_up) {
        log_debug(master->logger, 
                  "[cleanup_query_resources] Query ID=%d ya fue limpiada anteriormente",
                  qcb->query_id);
        return;
    }

    // Marco como "limpiada" ANTES de hacer cleanup
    qcb->cleaned_up = true;

    log_debug(master->logger, "[cleanup_query_resources] Limpiando recursos para Query ID=%d", qcb->query_id);

    // Remover de TODAS las listas (best-effort)
    if (master->queries_table) {
        list_remove_element(master->queries_table->ready_queue, qcb);
        list_remove_element(master->queries_table->running_list, qcb);
        list_remove_element(master->queries_table->completed_list, qcb);
        list_remove_element(master->queries_table->canceled_list, qcb);
        list_remove_element(master->queries_table->query_list, qcb);
    }

    if (qcb->query_file_path) {
        free(qcb->query_file_path);
        qcb->query_file_path = NULL;
    }
    
    free(qcb);
}

/**
 * cleanup_worker_resources
 *  - Remueve el worker de las listas y libera memoria asociada
 *  - Asume que caller posee el lock workers_table->worker_table_mutex
 */
void cleanup_worker_resources(t_worker_control_block *wcb, t_master *master) {
    if (!wcb || !master) return;

    log_debug(master->logger, "[cleanup_worker_resources] Limpiando recursos para Worker ID=%d (socket=%d)", 
              wcb->worker_id, wcb->socket_fd);

    // Remover de TODAS las listas
    if (master->workers_table) {
        list_remove_element(master->workers_table->idle_list, wcb);
        list_remove_element(master->workers_table->busy_list, wcb);
        list_remove_element(master->workers_table->worker_list, wcb);
        
        // Decrementar contador
        if (master->workers_table->total_workers_connected > 0) {
            master->workers_table->total_workers_connected--;
            master->multiprogramming_level = master->workers_table->total_workers_connected;
        }

        // Agregar a lista worker desconectados
        list_add(master->workers_table->disconnected_list, wcb);
    }

    if (wcb->ip_address) {
        free(wcb->ip_address);
    }
    
    free(wcb);
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

int handle_error_from_storage(t_package *required_package, int client_socket, t_master *master) {
    if (!master || !master->queries_table) return -1;

    log_debug(master->logger, "[handle_error_from_storage] Error report recibido desde worker socket %d", client_socket);

    buffer_reset_offset(required_package->buffer);
    uint32_t first_val;
    package_read_uint32(required_package, &first_val);

    // Bloquear AMBOS mutexes
    if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
        log_error(master->logger, "[handle_error_from_storage] Error al bloquear worker_table_mutex");
        return -1;
    }

    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, "[handle_error_from_storage] Error al bloquear query_table_mutex");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

/*     if (!package_read_uint32(required_package, &query_id)) {
        log_error(master->logger, "[handle_error_from_storage] Error al leer query ID del paquete");
        goto cleanup_and_exit;
    }
    
    if (query_id < 0) {
        log_error(master->logger, "[handle_error_from_storage] Query ID inválida: %d", query_id);
        goto cleanup_and_exit;
    } */

    char *error_msg = package_read_string(required_package);

/*     // Intentar encontrar worker por ese primer valor (podría ser worker_id)
    search_worker_id = (int)first_val;
    t_worker_control_block *wcb = list_find(master->workers_table->worker_list, match_worker_by_id);

    if (wcb) {
        // Formato con worker_id presente. Leer query_id a continuación.
        uint32_t worker_id = first_val;
        if (!package_read_uint32(required_package, &query_id)) {
            // Si no viene query_id, intentar obtenerlo desde el worker por socket
            log_warning(master->logger, "[handle_error_from_storage] No se encontró query_id en paquete tras worker_id; intentando obtener desde worker socket %d", client_socket);
            // buscar por socket
            wcb = find_worker_by_socket(master->workers_table, client_socket);
            if (!wcb) {
                log_error(master->logger, "[handle_error_from_storage] No se encontró worker con socket %d", client_socket);
                goto cleanup_and_exit;
            }
            query_id = (uint32_t) wcb->current_query_id;
        }
    } else {
        // El primer valor es probablemente query_id (formato {query_id, msg})
        query_id = first_val;
 */

    // Buscar worker por socket
    t_worker_control_block *wcb = find_worker_by_socket(master->workers_table, client_socket);
    if (!wcb) {
        log_error(master->logger, "[handle_error_from_storage] No se encontró worker con socket %d", client_socket);
        goto cleanup_and_exit;
    }

    int query_id = wcb->current_query_id;
    if (query_id < 0) {
        log_error(master->logger, "[handle_error_from_storage] Worker ID=%d no tiene una query asignada", wcb->worker_id);
        goto cleanup_and_exit;
    }

    printf("Worker ID=%d reportó error en Query ID=%u\n", wcb->worker_id, query_id);
    
    // Buscar query
    t_query_control_block *qcb = NULL;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *q = list_get(master->queries_table->running_list, i);
        if (q && q->query_id == query_id) {
            qcb = q;
            break;
        }
    }

    if (!qcb) {
        log_error(master->logger, "[handle_error_from_storage] Query ID=%u no encontrada en running_list", query_id);
        goto cleanup_and_exit;
    }

    log_warning(master->logger, "[handle_error_from_storage] Query ID=%u reportó ERROR: %s",
                query_id, error_msg ? error_msg : "(sin mensaje)");

/*     // Actualizar estados
    qcb->state = QUERY_STATE_CANCELED;
    qcb->assigned_worker_id = -1;
    
    // Remover de running_list
    list_remove_element(master->queries_table->running_list, qcb); */
    
    // Worker pasa a IDLE
    wcb->state = WORKER_STATE_IDLE;
    wcb->current_query_id = -1;
    list_remove_element(master->workers_table->busy_list, wcb);
    list_add(master->workers_table->idle_list, wcb);

    // Notificar QC y hacer cleanup CON los mutexes tomados
    finalize_query_with_error(qcb, master, error_msg ? error_msg : "Error en Storage");
    cleanup_query_resources(qcb, master);

cleanup_and_exit:
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    if (error_msg) free(error_msg);

    // Intentar despachar siguiente query
    try_dispatch(master);

    return 0;
}
