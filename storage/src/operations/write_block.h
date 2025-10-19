#ifndef STORAGE_OPERATIONS_WRITE_BLOCK_H_
#define STORAGE_OPERATIONS_WRITE_BLOCK_H_

#include "connection/serialization.h"
#include "connection/protocol.h"
#include "globals/globals.h"
#include "server/server.h"
#include "utils/filesystem_utils.h"

#define IN_PROGRESS "WORK_IN_PROGRESS"
#define COMMITTED "COMMITTED"

t_package* write_block(t_package *package);

#endif