#include "create_file.h"

t_package *create_file(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_FILE_CREATE_RES);
  return response;
}
