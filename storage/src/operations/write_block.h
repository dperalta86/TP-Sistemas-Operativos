#ifndef STORAGE_OPERATIONS_WRITE_BLOCK_H_
#define STORAGE_OPERATIONS_WRITE_BLOCK_H_

#include "connection/serialization.h"
#include "connection/protocol.h"
#include "globals/globals.h"
#include "server/server.h"

t_package* write_block(t_package *package);

#endif