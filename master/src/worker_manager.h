/**
 * @file worker_manager.h
 * @brief Definiciones para la gestión de Workers en el Master
 */

#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <pthread.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <connection/serialization.h>
#include <connection/protocol.h>

typedef struct master t_master;

typedef enum {
    WORKER_STATE_DISCONNECTED,
    WORKER_STATE_IDLE,
    WORKER_STATE_BUSY
} t_worker_state;

typedef struct {
    int worker_id;
    char *ip_address; // Ver si es necesasrio guardar la IP y el puerto
    int port;
    int socket_fd;
    int current_query_id;
    t_worker_state state;
} t_worker_control_block;

typedef struct worker_table {
    t_list *worker_list; // Lista de t_worker_control_block
    int total_workers_connected; // Define el nivel de multiprocesamiento

    t_list *idle_list; // Lista de workers en estado IDLE
    t_list *busy_list; // Lista de workers en estado BUSY
    t_list *disconnected_list; // Lista de workers en estado DISCONNECTED

    // Mutex para sincronización en asignación de workers
    pthread_mutex_t worker_table_mutex;
} t_worker_table;

/**
 * @brief Maneja el handshake inicial con un Worker.
 * 
 * @param buffer Buffer que contiene los datos del handshake.
 * @param client_socket Socket del cliente (Worker).
 * @param master Puntero a la estructura del Master.
 * @return int 0 si el handshake fue exitoso, -1 en caso de error.
 * 
 * TODO: Implementar almacenamiento del ID del Worker en la tabla de workers para luego planificar.
 */
int manage_worker_handshake(t_buffer *buffer, int client_socket, t_master *master);

int manage_read_message_from_worker(t_buffer *buffer, int client_socket, t_master *master);

/**
 * @brief Crea un nuevo Worker Control Block (WCB) y lo inicializa.
 * 
 * @param table Puntero a la tabla de control de workers.
 * @param worker_id ID del Worker como cadena.
 * @param socket_fd Descriptor de archivo del socket del Worker.
 * @return t_worker_control_block* Puntero al nuevo WCB creado, o NULL en caso de error.
 */
t_worker_control_block *create_worker(t_worker_table *table, char *worker_id, int socket_fd);

#endif