#ifndef QUERY_CONTROL_MANAGER_H
#define QUERY_CONTROL_MANAGER_H

#include <utils/serialization.h>
#include <utils/protocol.h>
#include <pthread.h>

typedef enum {
    QUERY_STATE_NEW,
    QUERY_STATE_READY,
    QUERY_STATE_RUNNING,
    QUERY_STATE_COMPLETED,
    QUERY_STATE_CANCELED
} t_query_state;

typedef struct {
    int query_id;
    char *query_file_path;
    int priority;
    int initial_priority;
    int assigned_worker_id;
    int program_counter;
    t_query_state state;
} t_query_control_block;

typedef struct {
    t_query_control_block *query_list; // Lista de t_query_control_block

    // Manejo de estados
    t_query_control_block *ready_queue;   // Cola de queries listas para ejecutar
    t_query_control_block *running_list; // Lista de queries en ejecución
    t_query_control_block *completed_list; // Lista de queries completadas
    t_query_control_block *canceled_list; // Lista de queries canceladas

    int total_queries;
    int next_query_id; // ID para la próxima query que se agregue

    // Mutex para sincronización en cambio de estados
    pthread_mutex_t query_table_mutex;
} t_query_table;

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

/**
 * @brief Maneja el proceso de handshake inicial con un cliente de control de consultas.
 *
 * Esta función envía un ID asignado (actualmente hardcodeado) al cliente para completar el proceso de handshake.
 * Registra cualquier error que ocurra durante el envío del ID.
 *
 * @param buffer Puntero al bufer que contiene los datos entrantes.
 * @param client_socket Descriptor de socket para el cliente conectado.
 * @param logger Pointer al logger para registrar eventos y errores.
 * @return 0 en caso de éxito, negativo en caso de error.
 */
int manage_query_handshake(t_buffer *buffer, int client_socket, t_log *logger);

#endif