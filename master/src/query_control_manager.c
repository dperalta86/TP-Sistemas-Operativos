#include "query_control_manager.h"
#include "utils/protocol.h"
#include "utils/serialization.h"

int manage_query_file_path(t_buffer *buffer, int client_socket, t_log *logger) {
    // Extraer path del query y prioridad del paquete
    buffer_reset_offset(buffer);
    char* data = buffer_read_string(buffer);
    if (data == NULL) {
        return -1;
    }

    char* delimiter_position = strchr(data, 0x1F); // 0x1F es el delimitador
    if (delimiter_position == NULL) {
        free(data);
        return -1;
    }

    *delimiter_position = '\0'; // Reemplazo el delimitador por un null terminator
    char* query_file_path = data;
    int priority = atoi(delimiter_position + 1);

    // Enviar OK como respuesta
    char* response = "PATH_RECEIVED_OK";
    if (send(client_socket, response, strlen(response), 0) == -1) 
    {
        log_error(logger, "Error al enviar respuesta de handshake al Query Control %d", client_socket);
        return -1;
    }

    // Loggear la información recibida
    log_info(logger, "## Se conecta un Query Control para ejecutar la Query path:%s con prioridad %d - Id asignado: %d. Nivel multiprocesamiento %d",query_file_path, priority, 0, 0);

    // TODO: agregar la lógica para manejar la ejecución del query

    free(data);
    return 0;

}