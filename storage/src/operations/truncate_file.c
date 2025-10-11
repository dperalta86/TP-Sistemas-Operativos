#include "truncate_file.h"

t_package *truncate_file(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_FILE_TRUNCATE_RES);
  return response;
}