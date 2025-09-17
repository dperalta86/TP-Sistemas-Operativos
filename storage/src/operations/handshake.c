#include "operations/handshake.h"

t_package* reply_handshake(void) {

    t_buffer *buffer = buffer_create(sizeof(uint8_t));    
    buffer_write_uint8(buffer, OP_CODE_RES_HANSHAKE);
    t_package *package = package_create(OP_CODE_RES_HANSHAKE, buffer);

    return package;
}