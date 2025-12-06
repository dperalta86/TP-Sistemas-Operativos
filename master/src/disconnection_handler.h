#ifndef DISCONNECTION_HANDLER_H
#define DISCONNECTION_HANDLER_H

#include "init_master.h"
#include "query_control_manager.h"
#include "worker_manager.h"
#include <commons/log.h>

// Maneja la desconexión de un Query Control
// Inicia proceso de cancelación de la query
// Retorna 0 en éxito, -1 en error
int handle_query_control_disconnection(int client_socket, t_master *master);

// Maneja la desconexión de un Worker
// Finaliza query en ejecución con error y notifica al QC
// Retorna 0 en éxito, -1 en error
int handle_worker_disconnection(int client_socket, t_master *master);

// Cancela una query que está en estado READY
// La envía directamente a EXIT
void cancel_query_in_ready(t_query_control_block *qcb, t_master *master);

// Cancela una query que está en estado EXEC
// Notifica al worker para desalojarla y espera contexto
int cancel_query_in_exec(t_query_control_block *qcb, t_master *master);

// Maneja la respuesta del worker al desalojo de query
// Mueve la query a EXIT tras recibir contexto
int handle_eviction_response(int worker_socket, t_buffer *buffer, t_master *master);

// Finaliza query con error y notifica al Query Control
void finalize_query_with_error(t_query_control_block *qcb, t_master *master, const char *error_reason);

// Función auxiliar para encontrar query por socket
t_query_control_block* find_query_by_socket(t_query_table *table, int socket_fd);

// Función auxiliar para encontrar worker por socket
t_worker_control_block* find_worker_by_socket(t_worker_table *table, int socket_fd);

// Limpia los recursos asociados a una query en EXIT
void cleanup_query_resources(t_query_control_block *qcb, t_master *master);

// Limpia los recursos asociados a un worker
void cleanup_worker_resources(t_worker_control_block *wcb, t_master *master);

// Maneja errores recibidos desde el Storage
int handle_error_from_storage(t_package *pkg, int storage_socket, t_master *master);

#endif // DISCONNECTION_HANDLER_H