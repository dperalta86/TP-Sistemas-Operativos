/**
 * @file query_control_manager.h
 * @brief Definiciones para la gestión de Query Controls en el Master
 */

#ifndef QUERY_CONTROL_MANAGER_H
#define QUERY_CONTROL_MANAGER_H

#include <connection/protocol.h>
#include <connection/serialization.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <commons/log.h>

// Forward declaration para evitar inclusiones circulares
// Se utiliza un puntero a esta estructura en las funciones
typedef struct master t_master;

typedef enum {
    QUERY_STATE_NEW,
    QUERY_STATE_READY,
    QUERY_STATE_RUNNING,
    QUERY_STATE_COMPLETED,
    QUERY_STATE_CANCELED
} t_query_state;

typedef struct {
    int socket_fd;
    int query_id;
    char *query_file_path;
    int priority;
    int initial_priority;
    uint64_t ready_timestamp;
    int assigned_worker_id;
    int program_counter;
    t_query_state state;
} t_query_control_block;

typedef struct query_table {
    t_list *query_list; // Lista de t_query_control_block

    // Manejo de estados
    t_list *ready_queue;   // Cola de queries listas para ejecutar
    t_list *running_list; // Lista de queries en ejecución
    t_list *completed_list; // Lista de queries completadas
    t_list *canceled_list; // Lista de queries canceladas

    int total_queries;
    int next_query_id; // ID para la próxima query que se agregue

    // Mutex para sincronización en cambio de estados
    pthread_mutex_t query_table_mutex;
} t_query_table;

int manage_query_file_path(t_package *response_package, int client_socket, t_master *master);

/**
 * @brief Genera y devuelve un ID único y secuencial para una nueva query.
 *
 * Esta función incrementa el contador de IDs de queries en la estructura del Master
 * y devuelve el nuevo ID asignado. Este ID se utiliza para identificar de manera única
 * cada query gestionada por el sistema.
 *
 * @param master Puntero a la estructura principal del Master que contiene la tabla de query.
 * @return El ID único y secuencial asignado a la nueva query.
 */
int generate_query_id(t_master *master);

/**
 * @brief Maneja el handshake inicial con un Query Control.
 *
 * Esta función envía un paquete de handshake al Query Control que se ha conectado.
 * Si el paquete no se puede crear o enviar, se registran errores en el log.
 *
 * @param client_socket El socket del cliente (Query Control) que se ha conectado.
 * @param logger Puntero al logger para registrar mensajes de log.
 * @return 0 si el handshake fue exitoso, -1 si hubo un error al crear el paquete,
 *         -2 si hubo un error al enviar el paquete.
 */
int manage_query_handshake(int client_socket, t_log *logger);

/**
 * @brief Crea y inicializa un nuevo bloque de control de query (QCB).
 * 
 * Luego de crear la estructura, la agrega a la tabla de queries y a la cola de ready.
 */
t_query_control_block *create_query(t_master *master, int query_id, char *query_file_path, int priority, int socket_fd);

/**
 * @brief Inserta una query en la cola de ready respetando el orden por prioridad.
 *
 * Esta función recorre la cola de ready (`ready_queue`) y ubica la nueva query (`new_qcb`)
 * en la posición correspondiente según su prioridad. Las prioridades más bajas indican
 * mayor prioridad de ejecución. Si no hay ninguna query con menor prioridad, se agrega al final.
 *
 * @param ready_queue Puntero a la lista de queries en estado READY.
 * @param new_qcb Puntero al bloque de control de la nueva query a insertar.
 * @return int Devuelve 0 en caso de éxito, o -1 si ocurre un error (por ejemplo, parámetros nulos).
 */
int insert_query_by_priority(t_list *ready_queue, t_query_control_block *new_qcb);


uint64_t now_ms_monotonic();

#endif // QUERY_CONTROL_MANAGER_H