#include "write_block.h"

t_package *write_block(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_BLOCK_WRITE_RES);
  return response;
}