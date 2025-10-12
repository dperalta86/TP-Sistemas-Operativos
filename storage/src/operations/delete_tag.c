#include "delete_tag.h"

t_package *delete_tag(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_TAG_DELETE_RES);
  return response;
}