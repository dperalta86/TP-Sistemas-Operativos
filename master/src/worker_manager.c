#include "worker_manager.h"
#include "query_control_manager.h"
#include <init_master.h>
#include "connection/protocol.h"
#include "connection/serialization.h"
#include "scheduler.h"
#include <commons/collections/list.h>
#include <unistd.h>
#include "disconnection_handler.h"


int manage_worker_handshake(t_buffer *buffer, int client_socket, t_master *master) {
    // Recibo el ID del Worker
    //buffer_reset_offset(buffer);
    uint32_t worker_id;
    buffer_read_uint32(buffer, &worker_id);
    if (worker_id <= 0) {
        log_error(master->logger, "ID de Worker inválido recibido en handshake");
        return -1;
    }

    // Registro el Worker en la tabla de control
    t_worker_control_block *wcb = create_worker(master->workers_table, worker_id, client_socket);
    if (wcb == NULL) {
        log_error(master->logger, "Error al crear el control block para Worker ID: %d", worker_id);

        return -1;
    }
    log_info(master->logger, "Worker ID: %d registrado exitosamente", worker_id);
    master->multiprogramming_level = master->workers_table->total_workers_connected;
    log_debug(master->logger, "Total Workers conectados: %d", master->workers_table->total_workers_connected);


    t_package* response = package_create(OP_WORKER_ACK, buffer); // Para no enviar buffer vacío
    if (package_send(response, client_socket) != 0)
    {
        log_error(master->logger, "Error al enviar respuesta de handshake al Worker id: %d - socket: %d", worker_id, client_socket);
        package_destroy(response);
        return -1;
    }

    // Envío ACK al Worker
    log_info(master->logger, "Handshake recibido de Worker ID: %d", worker_id);

    // Verifico si hay queries pendientes para asignar
    if(try_dispatch(master)!=0)
    {
        log_error(master->logger, "Error al intentar despachar una query luego del handshake del Worker id: %d - socket: %d", worker_id, client_socket);
    }

    return 0;
}

int manage_read_message_from_worker(t_buffer *buffer, int client_socket, t_master *master) {
    if (!buffer || !master) {
        log_error(master ? master->logger : NULL, "[manage_read_message_from_worker] Parámetros inválidos (buffer o master NULL).");
        return -1;
    }

    uint32_t worker_id;
    uint32_t query_id;
    void *data = NULL;
    size_t size;

    buffer_reset_offset(buffer);
    buffer_read_uint32(buffer, &worker_id);
    buffer_read_uint32(buffer, &query_id);
    data = buffer_read_data(buffer, &size);
    char *file = buffer_read_string(buffer);
    char *tag = buffer_read_string(buffer);

    // Calculo tamaño necesario y asignar memoria adecuada (QC espera un unico string "FILE:TAG")
    size_t file_tag_len = strlen(file) + 1 + strlen(tag) + 1;
    char *file_tag = malloc(file_tag_len);

    // Construir "FILE:TAG"
    snprintf(file_tag, file_tag_len, "%s:%s", file, tag);
    
    // Libero los strings temporales
    free(file);
    free(tag);

    log_debug(master->logger, "Recibido lectura desde worker id: %d para renviar a query id: %d. File:Tag <%s> Data= %s", worker_id, query_id, file_tag, (char*)data);

    // Buscar la query correspondiente al ID
    t_query_control_block *query = NULL;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *aux_qcb = list_get(master->queries_table->running_list, i);
        if (aux_qcb->query_id == query_id) {
            query = aux_qcb;
            break;
        }
    }

    if (query == NULL) {
        log_error(master->logger, "[manage_read_message_from_worker] No se encontró query ID=%d asociada al worker ID=%d.",
                  query_id, worker_id);
        try_dispatch(master); // Intentar despachar otras queries pendientes
        return -1;
    }


    // Crear paquete a reenviar al Query Control
    t_package *package_to_query = package_create_empty(QC_OP_READ_DATA);
    if (!package_to_query) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al crear paquete para reenviar a Query Control");
        return -1;
    }

    // Copiar el contenido del buffer recibido del Worker al nuevo paquete
    if (!(package_add_data(package_to_query, data, size) && package_add_string(package_to_query, file_tag))) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al copiar buffer de Worker ID=%d hacia Query ID=%d.",
                  worker_id, query_id);
        package_destroy(package_to_query);
        return -1;
    }

    // Enviar el paquete al Query Control
    if (package_send(package_to_query, query->socket_fd) != 0) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al reenviar mensaje de Worker ID=%d hacia Query ID=%d (socket=%d).",
                  worker_id, query->query_id, query->socket_fd);
        package_destroy(package_to_query);
        return -1;
    }

    log_debug(master->logger, "[manage_read_message_from_worker] Mensaje reenviado de Worker ID=%d → Query ID=%d correctamente.",
              worker_id, query->query_id);

    // Liberar memoria
    package_destroy(package_to_query);
    free(file_tag);
    free(data);
    return 0;
}


// Crea un nuevo WCB y lo inicializa
t_worker_control_block *create_worker(t_worker_table *table, int worker_id, int socket_fd) {
    // loqueamos la tabla para manipular datos administrativos
    pthread_mutex_lock(&table->worker_table_mutex);

    t_worker_control_block *wcb = malloc(sizeof(t_worker_control_block));
    wcb->worker_id = worker_id;
    wcb->ip_address = NULL; // TODO: Obtener IP del socket (entiendo que con el socket ya es suficiente)
    wcb->port = -1; // TODO: Obtener puerto del socket
    wcb->socket_fd = socket_fd; // Necesito el socket para enviar las queries
    wcb->current_query_id = -1; // No tiene query asignada inicialmente
    wcb->state = WORKER_STATE_IDLE; // Nuevo worker comienza en estado IDLE

    // Agrego el puntero a la lista de workers
    list_add(table->worker_list, wcb);
    table->total_workers_connected++;
    list_add(table->idle_list, wcb); // Nuevo worker comienza en estado IDLE
    pthread_mutex_unlock(&table->worker_table_mutex);
    return wcb;
}

// Maneja la respuesta de desalojo enviada por un Worker
void manage_worker_evict_response(int socket_fd, t_package *package, t_master *master) {
    if (!package || !master) {
        log_error(master ? master->logger : NULL, 
                  "[manage_worker_evict_response] Parámetros inválidos");
        return;
    }

    uint32_t query_id, program_counter;
    
    // Leer datos del paquete
    if (!package_read_uint32(package, &query_id)) {
        log_error(master->logger, 
                  "[manage_worker_evict_response] Error al leer query_id del paquete");
        return;
    }
    
    if (!package_read_uint32(package, &program_counter)) {
        log_error(master->logger, 
                  "[manage_worker_evict_response] Error al leer program_counter del paquete");
        return;
    }

    log_debug(master->logger, 
              "[manage_worker_evict_response] Recibida respuesta de desalojo - Query ID=%d, PC=%d (socket=%d)",
              query_id, program_counter, socket_fd);

    // Bloquear ambas tablas en orden consistente
    if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
        log_error(master->logger, 
                  "[manage_worker_evict_response] Error al bloquear mutex de workers");
        return;
    }

    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, 
                  "[manage_worker_evict_response] Error al bloquear mutex de queries");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return;
    }

    // Buscar el worker que envió la respuesta
    t_worker_control_block *worker = NULL;
    for (int i = 0; i < list_size(master->workers_table->worker_list); i++) {
        t_worker_control_block *w = list_get(master->workers_table->worker_list, i);
        if (w && w->socket_fd == socket_fd) {
            worker = w;
            break;
        }
    }

    if (!worker) {
        log_warning(master->logger, 
                    "[manage_worker_evict_response] No se encontró worker con socket=%d",
                    socket_fd);
        goto unlock_and_exit;
    }

    // Buscar la query desalojada
    t_query_control_block *query = NULL;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *q = list_get(master->queries_table->running_list, i);
        if (q && q->query_id == query_id) {
            query = q;
            break;
        }
    }

    if (!query) {
        log_warning(master->logger, 
                    "[manage_worker_evict_response] No se encontró Query ID=%d en running_list",
                    query_id);
        goto unlock_and_exit;
    }

    // Verificar consistencia
    if (query->assigned_worker_id != worker->worker_id) {
        log_warning(master->logger, 
                    "[manage_worker_evict_response] Inconsistencia: Query ID=%d estaba asignada al Worker ID=%d, pero respondió Worker ID=%d",
                    query_id, query->assigned_worker_id, worker->worker_id);
    }

    // Verificar si la query fue CANCELADA mientras esperábamos respuesta
    bool query_was_canceled = (query->state == QUERY_STATE_CANCELED);
    
    if (query_was_canceled) {
        log_info(master->logger, 
                 "## Query ID=%d fue cancelada durante el desalojo - No se re-encola (PC=%d)",
                 query_id, program_counter);
        
        // Actualizar contexto y limpiar
        query->program_counter = program_counter;
        query->preemption_pending = false;
        query->assigned_worker_id = -1;
        
        // Remover de running_list
        if (!list_remove_element(master->queries_table->running_list, query)) {
            log_warning(master->logger, 
                        "[manage_worker_evict_response] Query ID=%d no estaba en running_list",
                        query_id);
        }
        
        // hacer cleanup directo
        cleanup_query_resources(query, master);
        
    } else {
        // Desalojo normal por prioridad - re-encolar
        query->program_counter = program_counter;
        query->state = QUERY_STATE_READY;
        query->assigned_worker_id = -1;
        query->preemption_pending = false;
        query->ready_timestamp = now_ms_monotonic();

        // Mover query de running_list a ready_queue
        if (!list_remove_element(master->queries_table->running_list, query)) {
            log_error(master->logger, 
                      "[manage_worker_evict_response] Error al remover Query ID=%d de running_list",
                      query_id);
            goto unlock_and_exit;
        }

        if (insert_query_by_priority(master->queries_table->ready_queue, query) != 0) {
            log_error(master->logger, "Error al intentar insertar query (query ID: %d) en Ready Queue.", query_id);
            // Si falla el insert, hacer cleanup para no perder la query
            cleanup_query_resources(query, master);
            goto unlock_and_exit;
        }

        log_info(master->logger, 
                 "## Query ID=%d desalojada exitosamente del Worker ID=%d - PC guardado: %d - Vuelta a READY",
                 query_id, worker->worker_id, program_counter);
    }

    // Actualizar estado del worker (siempre, independiente de si query fue cancelada)
    worker->current_query_id = -1;
    worker->state = WORKER_STATE_IDLE;

    // Mover worker de busy_list a idle_list
    if (!list_remove_element(master->workers_table->busy_list, worker)) {
        log_error(master->logger, 
                  "[manage_worker_evict_response] Error al remover Worker ID=%d de busy_list",
                  worker->worker_id);
        goto unlock_and_exit;
    }

    list_add(master->workers_table->idle_list, worker);

    log_debug(master->logger, 
              "[manage_worker_evict_response] Worker ID=%d liberado y movido a IDLE",
              worker->worker_id);

unlock_and_exit:
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    // Intentar despachar queries pendientes
    try_dispatch(master);
}

int manage_worker_end_query(t_buffer *buffer, int client_socket, t_master *master) {
    if (!buffer || !master) {
        log_error(master ? master->logger : NULL, "[manage_worker_end_query] Parámetros inválidos.");
        return -1;
    }

    uint32_t worker_id, query_id;
    buffer_reset_offset(buffer);
    buffer_read_uint32(buffer, &worker_id);
    buffer_read_uint32(buffer, &query_id);

    // Bloquear AMBOS mutexes en ORDEN
    pthread_mutex_lock(&master->workers_table->worker_table_mutex);
    pthread_mutex_lock(&master->queries_table->query_table_mutex);

    // Buscar el worker
    t_worker_control_block *worker = NULL;
    for (int i = 0; i < list_size(master->workers_table->worker_list); i++) {
        t_worker_control_block *aux = list_get(master->workers_table->worker_list, i);
        if (aux->socket_fd == client_socket) {
            worker = aux;
            break;
        }
    }

    if (worker == NULL) {
        log_error(master->logger, 
                  "[manage_worker_end_query] No se encontró worker para socket=%d (worker_id=%u).", 
                  client_socket, worker_id);
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

    // Buscar la query en running_list
    t_query_control_block *qcb = NULL;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *aux = list_get(master->queries_table->running_list, i);
        if (aux->query_id == (int)query_id) {
            qcb = aux;
            break;
        }
    }

    if (qcb == NULL) {
        log_error(master->logger, 
                  "[manage_worker_end_query] No se encontró Query ID=%u en RUNNING (Worker ID=%u).", 
                  query_id, worker->worker_id);
        
        // Liberar worker por seguridad
        worker->state = WORKER_STATE_IDLE;
        worker->current_query_id = -1;
        
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

    log_info(master->logger,
             "## Se terminó la Query %d en el Worker %d",
             qcb->query_id, worker->worker_id);

    // Verificar si ya fue limpiada
    if (qcb->cleaned_up) {
        log_debug(master->logger,
                  "[manage_worker_end_query] Query ID=%d ya fue limpiada, solo liberar worker",
                  qcb->query_id);
        
        worker->state = WORKER_STATE_IDLE;
        worker->current_query_id = -1;
        list_remove_element(master->workers_table->busy_list, worker);
        list_add(master->workers_table->idle_list, worker);
        
        pthread_mutex_unlock(&master->queries_table->query_table_mutex);
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        
        try_dispatch(master);
        return 0;
    }

    // Actualizar estados
    qcb->state = QUERY_STATE_COMPLETED;
    qcb->assigned_worker_id = -1;
    
    list_remove_element(master->queries_table->running_list, qcb);

    worker->state = WORKER_STATE_IDLE;
    worker->current_query_id = -1;
    
    list_remove_element(master->workers_table->busy_list, worker);
    list_add(master->workers_table->idle_list, worker);

    // Notificar al QC CON mutexes tomados
    int qc_socket = qcb->socket_fd;
    if (qc_socket > 0) {
        t_package *resp = package_create_empty(OP_END_QUERY);
        if (resp) {
            package_add_uint32(resp, (uint32_t)qcb->query_id);
            if (package_send(resp, qc_socket) != 0) {
                log_error(master->logger, 
                          "[manage_worker_end_query] Error al enviar resultado final a QC (Query ID=%d).",
                          qcb->query_id);
            }
            package_destroy(resp);
        }
    }

    // Cleanup CON mutexes tomados
    cleanup_query_resources(qcb, master);

    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    // Intentar despachar siguiente query
    try_dispatch(master);

    return 0;
}