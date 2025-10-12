#include "worker_manager.h"
#include <init_master.h>
#include "connection/protocol.h"
#include "connection/serialization.h"
#include "scheduler.h"
#include <commons/collections/list.h>


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

    // Buscar el worker asociado a este socket (recorro lista de workers registrados)
    t_worker_control_block *worker = NULL;
    for (int i = 0; i < list_size(master->workers_table->worker_list); i++) {
        t_worker_control_block *aux_wcb = list_get(master->workers_table->worker_list, i);
        if (aux_wcb->socket_fd == client_socket) {
            worker = aux_wcb;
            break;
        }
    }

    if (worker == NULL) {
        log_error(master->logger, "[manage_read_message_from_worker] No se encontró worker asociado al socket %d.", client_socket);
        return -1;
    }

    if (worker->current_query_id < 0) {
        log_warning(master->logger, "[manage_read_message_from_worker] Worker ID=%d no tiene query asociada actualmente.", worker->worker_id);
        return 0;
    }

    // Buscar la query correspondiente al ID
    t_query_control_block *query = NULL;
    for (int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *aux_qcb = list_get(master->queries_table->running_list, i);
        if (aux_qcb->query_id == worker->current_query_id) {
            query = aux_qcb;
            break;
        }
    }

    if (query == NULL) {
        log_error(master->logger, "[manage_read_message_from_worker] No se encontró query ID=%d asociada al worker ID=%d.",
                  worker->current_query_id, worker->worker_id);
        return -1;
    }

    // Crear paquete a reenviar al Query Control
    t_package *package_to_query = package_create_empty(QC_OP_READ_DATA);
    if (!package_to_query) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al crear paquete para reenviar a Query Control (Query ID=%d).",
                  query->query_id);
        return -1;
    }

    // Copiar el contenido del buffer recibido del Worker al nuevo paquete
    // Asumo que el buffer contiene un string (mensaje leido).
    // TODO: Verificar si el buffer puede contener otro tipo de datos, en tal caso usar:
    // package_add_data()
    buffer_reset_offset(buffer);
    char *msg = buffer_read_string(buffer);
    if (msg == NULL) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al leer string del buffer del Worker ID=%d.",
                  worker->worker_id);
        package_destroy(package_to_query);
        return -1;
    }
    if (package_add_string(package_to_query, msg) != 0) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al copiar buffer de Worker ID=%d hacia Query ID=%d.",
                  worker->worker_id, query->query_id);
        package_destroy(package_to_query);
        return -1;
    }

    // Enviar el paquete al Query Control
    if (package_send(package_to_query, query->socket_fd) != 0) {
        log_error(master->logger, "[manage_read_message_from_worker] Error al reenviar mensaje de Worker ID=%d hacia Query ID=%d (socket=%d).",
                  worker->worker_id, query->query_id, query->socket_fd);
        package_destroy(package_to_query);
        return -1;
    }

    log_debug(master->logger, "[manage_read_message_from_worker] Mensaje reenviado de Worker ID=%d → Query ID=%d correctamente.",
              worker->worker_id, query->query_id);

    // Liberar memoria
    package_destroy(package_to_query);
    free(msg);
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