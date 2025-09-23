#include "master.h"

int handshake_with_master(const char *master_ip,
                          const char *master_port,
                          char *worker_id)
{
    return handshake_with_server("Master",
                                 master_ip,
                                 master_port,
                                 OP_WORKER_HANDSHAKE_REQ,
                                 OP_WORKER_HANDSHAKE_RES,
                                 worker_id);
}
