#include "create_tag.h"

t_package *create_tag(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_TAG_CREATE_RES);
  return response;
}