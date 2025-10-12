#include "commit_tag.h"

t_package *commit_tag(t_package *package) {
  t_package *response = package_create_empty(STORAGE_OP_TAG_COMMIT_RES);
  return response;
}