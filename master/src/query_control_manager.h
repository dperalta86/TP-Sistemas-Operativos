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

int manage_query_file_path(t_buffer *required_package,int client_socket, t_log *logger);

#endif