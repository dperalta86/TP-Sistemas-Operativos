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
    int query_id;
    char *query_file_path;
    int priority;
    int initial_priority;
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

int manage_query_handshake(int client_socket, t_log *logger);

#endif