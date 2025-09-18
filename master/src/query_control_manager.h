#ifndef QUERY_CONTROL_MANAGER_H
#define QUERY_CONTROL_MANAGER_H

#include <commons/log.h>
#include <utils/serialization.h>
#include <utils/protocol.h>

typedef enum {
    QUERY_STATE_NEW,
    QUERY_STATE_READY,
    QUERY_STATE_RUNNING,
    QUERY_STATE_COMPLETED,
    QUERY_STATE_CANCELED
} t_query_state;

typedef struct {
    int query_id;
    int client_socket;
    char *query_file_path;
    int priority;
    int asigned_worker_id;
    t_query_state state;
    t_log *logger;
} t_query_control_block;

/**
 * @brief Maneja la recepción y el procesamiento de la ruta de archivo de consulta y su prioridad desde un cliente.
 *
 * Esta función lee una cadena del búfer proporcionado, que contiene la ruta del archivo de consulta y su prioridad,
 * separadas por el delimitador 0x1F. Analiza la ruta y la prioridad, envía una respuesta de confirmación al cliente,
 * registra la información recibida y prepara el manejo posterior de la ejecución de la consulta.
 *
 * @param buffer Puntero al bufer que contiene los datos entrantes.
 * @param client_socket Descriptor de socket para el cliente conectado.
 * @param logger Pointer al logger para registrar eventos y errores.
 * @return 0 en caso de éxito, negativo en caso de error.
 */
int manage_query_file_path(t_buffer *required_package,int client_socket, t_log *logger);

#endif