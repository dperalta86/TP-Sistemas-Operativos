#include "worker_manager.h"
#include <init_master.h>
#include "connection/protocol.h"
#include "connection/serialization.h"
#include "scheduler.h"
#include <commons/collections/list.h>
#include <unistd.h>


int manage_worker_handshake(t_buffer *buffer, int client_socket, t_master *master) {
    // Recibo el ID del Worker
    buffer_reset_offset(buffer);
    char *worker_id = buffer_read_string(buffer);
    if (worker_id == NULL) {
        log_error(master->logger, "ID de Worker inválido recibido en handshake");
        return -1;
    }

    // Registro el Worker en la tabla de control
    t_worker_control_block *wcb = create_worker(master->workers_table, worker_id, client_socket);
    if (wcb == NULL) {
        log_error(master->logger, "Error al crear el control block para Worker ID: %s", worker_id);
        free(worker_id);
        return -1;
    }
    log_info(master->logger, "Worker ID: %s registrado exitosamente", worker_id);
    master->multiprogramming_level = master->workers_table->total_workers_connected;
    log_debug(master->logger, "Total Workers conectados: %d", master->workers_table->total_workers_connected);

    // Envío ACK al Worker
    log_info(master->logger, "Handshake recibido de Worker ID: %s", worker_id);
    t_package* response = package_create(OP_WORKER_ACK, buffer); // Para no enviar buffer vacío
    if (package_send(response, client_socket) != 0)
    {
        log_error(master->logger, "Error al enviar respuesta de handshake al Worker id: %s - socket: %d", worker_id, client_socket);
        package_destroy(response);
        return -1;
    }

    // Verifico si hay queries pendientes para asignar
    if(try_dispatch(master)!=0)
    {
        log_error(master->logger, "Error al intentar despachar una query luego del handshake del Worker id: %s - socket: %d", worker_id, client_socket);
    }

    // Libero recursos
    if(worker_id)
    {
        free(worker_id);
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
    char *file_tag;
    void *data = NULL;
    size_t size;

    buffer_reset_offset(buffer);
    buffer_read_uint32(buffer, &worker_id);
    buffer_read_uint32(buffer, &query_id);
    data = buffer_read_data(buffer, &size);
    file_tag = buffer_read_string(buffer);
    strcat(file_tag, ":");
    strcat(file_tag, buffer_read_string(buffer));

    log_debug(master->logger, "Recibido lectura desde worker id: %d para renviar a query id: %d. File:Tag <%s> Data= %s", worker_id, query_id, file_tag, data);

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
t_worker_control_block *create_worker(t_worker_table *table, char *worker_id, int socket_fd) {
    // loqueamos la tabla para manipular datos administrativos
    pthread_mutex_lock(&table->worker_table_mutex);

    t_worker_control_block *wcb = malloc(sizeof(t_worker_control_block));
    wcb->worker_id = atoi(worker_id);
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

int manage_worker_end_query(t_buffer *buffer, int client_socket, t_master *master) {
    if (!buffer || !master) {
        log_error(master ? master->logger : NULL, "[manage_worker_end_query] Parámetros inválidos.");
        return -1;
    }

    // Leer datos del paquete
    uint32_t worker_id, query_id;
    buffer_reset_offset(buffer);
    buffer_read_uint32(buffer, &worker_id);
    buffer_read_uint32(buffer, &query_id);

    t_worker_control_block *worker = NULL;
    for (int i = 0; i < list_size(master->workers_table->worker_list); i++) {
        t_worker_control_block *aux = list_get(master->workers_table->worker_list, i);
        if (aux->socket_fd == client_socket) {
            worker = aux;
            break;
        }
    }

    if (worker == NULL) {
        log_error(master->logger, "[manage_worker_end_query] No se encontró worker para socket=%d (worker_id=%u).", client_socket, worker_id);        return -1;
    }

    // Buscar la query en la running_list
    t_query_control_block *qcb = NULL;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *aux = list_get(master->queries_table->running_list, i);
        if (aux->query_id == (int)query_id) {
            qcb = aux;
            break;
        }
    }

    if (qcb == NULL) {
        log_error(master->logger, "[manage_worker_end_query] No se encontró Query ID=%u en RUNNING (Worker ID=%u).", query_id, worker->worker_id);
        // Marcar worker IDLE por seguridad
        worker->state = WORKER_STATE_IDLE;
        worker->current_query_id = -1;
        return -1;
    }

    // Log obligatorio
    log_info(master->logger,
             "## Se terminó la Query %d en el Worker %d",
             qcb->query_id, worker->worker_id);

    // Notificar al Query Control (si está conectado)
    if (qcb->socket_fd > 0) {
        t_package *resp = package_create_empty(OP_MASTER_QUERY_END);
        if (resp) {
            package_add_uint32(resp, (uint32_t) qcb->query_id);
            if (package_send(resp, qcb->socket_fd) != 0) {
                log_error(master->logger, "[manage_worker_end_query] Error al enviar resultado final a QC (Query ID=%d, socket=%d).",
                          qcb->query_id, qcb->socket_fd);
            }
            package_destroy(resp);
        }
        close(qcb->socket_fd); // Libero el socket
    }

    // TODO: Sacar de READY queue y limpiar recursos
    pthread_mutex_lock(&master->queries_table->query_table_mutex);
    qcb->state = QUERY_STATE_COMPLETED;
    list_remove_element(master->queries_table->ready_queue, qcb);
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    
    // Responder ACK al Worker
    t_package *ack = package_create_empty(OP_WORKER_ACK);
    if (ack) {
        package_send(ack, client_socket);
        package_destroy(ack);
    }

    // Poner worker IDLE y limpiar current_query_id
    pthread_mutex_lock(&master->workers_table->worker_table_mutex);
    worker->state = WORKER_STATE_IDLE;
    worker->current_query_id = -1;
    list_add(master->workers_table->idle_list, worker);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

    // Intentar despachar la siguiente query
    try_dispatch(master);

    return 0;
}