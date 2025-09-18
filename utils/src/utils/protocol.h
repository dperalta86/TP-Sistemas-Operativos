#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
    OP_QUERY_HANDSHAKE,
    OP_QUERY_FILE_PATH,
    OP_WORKER_HANDSHAKE_REQ,
    OP_WORKER_HANDSHAKE_RES,
    OP_WORKER_SEND_ID_REQ,
    OP_WORKER_SEND_ID_RES,
} t_master_op_code;

typedef enum {
    STORAGE_OP_WORKER_SEND_ID_REQ,
    STORAGE_OP_WORKER_SEND_ID_RES,
} t_storage_op_code;

#endif
