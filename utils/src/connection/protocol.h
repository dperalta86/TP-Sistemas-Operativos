#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Operation codes para Master
typedef enum {
    OP_QUERY_HANDSHAKE,
    OP_QUERY_FILE_PATH,
    OP_WORKER_HANDSHAKE,
    OP_WORKER_ACK,
    OP_WORKER_START_QUERY,
    OP_WORKER_READ_MESSAGE_REQ,
    QC_OP_READ_DATA,
} t_master_op_code;

typedef enum {
    STORAGE_OP_WORKER_SEND_ID_REQ,
    STORAGE_OP_WORKER_SEND_ID_RES,
    STORAGE_OP_WORKER_GET_BLOCK_SIZE_REQ,
    STORAGE_OP_WORKER_GET_BLOCK_SIZE_RES,
} t_storage_op_code;

#endif
