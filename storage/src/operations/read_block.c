#include "read_block.h"

t_package *read_block(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_BLOCK_READ_RES);
  return response;
}