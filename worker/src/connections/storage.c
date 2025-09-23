#include "storage.h"

int handshake_with_storage(const char *storage_ip,
                           const char *storage_port,
                           char *worker_id)
{
    return handshake_with_server("Storage",
                                 storage_ip, storage_port,
                                 STORAGE_OP_WORKER_SEND_ID_REQ,
                                 STORAGE_OP_WORKER_SEND_ID_RES,
                                 worker_id);
}
