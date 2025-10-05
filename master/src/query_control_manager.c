#include "init_master.h"
#include "query_control_manager.h"
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
    
    // TODO: agregar la lógica para manejar la ejecución del query

    //Una vez que se añade la logica para la ejecucion de query, armar el package de QC_OP_READ_FILE
    //t_package *query_path_package = package_create_empty(QC_OP_READ_FILE));


    package_destroy(query_path_package);
    free(query_path);
    return 0;
disconnect:
// TODO: manejar desconexion de QC
log_error(master->logger, "Error al enviar respuesta a Query Control, se desconectará...");
return -1;

}

int generate_query_id(t_master *master) {
    return ++(master->queries_table->next_query_id);
}