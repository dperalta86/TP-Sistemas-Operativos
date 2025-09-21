#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Operation codes para Master
typedef enum {
    OP_QUERY_HANDSHAKE,
    OP_QUERY_FILE_PATH,
    OP_WORKER_HANDSHAKE_REQ,
    OP_WORKER_HANDSHAKE_RES,
} t_master_op_code;

// TODO: Crear t_storage_op_code

#endif
