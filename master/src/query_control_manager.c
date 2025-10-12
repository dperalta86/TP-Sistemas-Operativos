#include "init_master.h"
#include "query_control_manager.h"
#include "scheduler.h"
#include "connection/protocol.h"
#include "connection/serialization.h"
#include "commons/log.h"

int manage_query_handshake(int client_socket, t_log *logger) {
    t_package *response_package = package_create_empty(OP_QUERY_HANDSHAKE);
    if (!response_package)
    {
        log_error(logger, "Error al crear package...");
        return -1;
    }
    if (package_send(response_package, client_socket) < 0) // --> package_create_empty se encarga de no cargar NULL
    {
        return -2;
    }
    return 0;
}

int manage_query_file_path(t_package *response_package, int client_socket, t_master *master) {
    // Extraer path del query y prioridad del paquete
    if (!response_package)
    {
        return -1;
    }
    char *query_path = package_read_string(response_package);

    uint8_t query_priority;
    package_read_uint8(response_package, &query_priority);

    int assigned_id = generate_query_id(master);
    int multiprocessing_level = master->multiprogramming_level; // TODO: Asignar el nivel real cuando esté implementado
    
    // Responder a QC
    t_package *query_path_package = package_create_empty(OP_QUERY_FILE_PATH);

    if (!query_path_package)
    {
        goto disconnect;
    }

    package_send(query_path_package, client_socket);

    // Loggear la información recibida
    log_info(master->logger, "## Se conecta un Query Control para ejecutar la Query path:%s con prioridad %d - Id asignado: %d. Nivel multiprocesamiento %d", query_path, query_priority, assigned_id, multiprocessing_level);
    
    // Agregar a la tabla de queries
    t_query_control_block *qcb = create_query(master->queries_table, assigned_id, query_path, query_priority, client_socket);
    if(!qcb)
    {
        log_error(master->logger, "Error al crear el control block para Query ID: %d", assigned_id);
        goto disconnect;
    }

    // Verifico si hay workers disponibles para asignar la query
    if(try_dispatch(master)!=0)
    {
        log_error(master->logger, "Error al intentar despachar una query luego del handshake del query id: %d - socket: %d", assigned_id, client_socket);
    }
    
    free(query_path);
    package_destroy(query_path_package);
    return 0;
disconnect:
// TODO: manejar desconexion de QC
log_error(master->logger, "Error al enviar respuesta a Query Control, se desconectará...");
return -1;

}

int generate_query_id(t_master *master) {
    return ++(master->queries_table->next_query_id);
}

t_query_control_block *create_query(t_query_table *table, int query_id, char *query_file_path, int priority, int socket_fd) {
    // loqueamos la tabla para manipular datos administrativos
    pthread_mutex_lock(&table->query_table_mutex);

    t_query_control_block *qcb = malloc(sizeof(t_query_control_block));
    qcb->socket_fd = socket_fd;
    qcb->query_id = query_id;
    qcb->query_file_path = strdup(query_file_path);
    qcb->priority = priority;
    qcb->initial_priority = priority;
    qcb->assigned_worker_id = -1; // Debería ser -1 al principio (sin asignar)
    qcb->program_counter = 0; // Inicia en 0, luego lo actualiza con datos desde el Worker
    qcb->state = QUERY_STATE_READY; // Inicia en READY al ser creada

    // Agregamos a la lista principal y a la cola de ready
    list_add(table->query_list, qcb);
    list_add(table->ready_queue, qcb);
    table->total_queries++;

    pthread_mutex_unlock(&table->query_table_mutex);
    return qcb;
}