#include "worker_manager.h"
#include "utils/protocol.h"
#include "utils/serialization.h"
#include <init_master.h>

int manage_worker_handshake(t_buffer *buffer, int client_socket, t_master *master) {
    // Recibo el ID del Worker
    buffer_reset_offset(buffer);
    char *worker_id = buffer_read_string(buffer); // TODO: Guardar el ID en la tabla de workers
    if (worker_id == NULL) {
        log_error(master->logger, "ID de Worker inválido recibido en handshake");
        return -1;
    }
    log_info(master->logger, "Handshake recibido de Worker ID: %s", worker_id);
    t_package* response = package_create(OP_WORKER_HANDSHAKE_RES, buffer); // Para no enviar buffer vacío
    if (package_send(response, client_socket) != 0)
    {
        log_error(master->logger, "Error al enviar respuesta de handshake al Worker id: %s - socket: %d", worker_id, client_socket);
        return -1;
    }
    return 0;
}