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