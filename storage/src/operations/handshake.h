#ifndef STORAGE_OPERATIONS_HANDSHAKE_H_
#define STORAGE_OPERATIONS_HANDSHAKE_H_

#include "utils/serialization.h"
#include "utils/protocol.h"
#include "globals/globals.h"
#include "server/server.h"

t_package* handle_handshake(t_package *package, t_client_data *client_data);

#endif