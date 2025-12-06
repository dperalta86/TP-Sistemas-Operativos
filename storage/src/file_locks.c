#include "file_locks.h"
#include "globals/globals.h"
#include <assert.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <stdlib.h>

void lock_file(const char *name, const char *tag, bool for_write) {
  char *key = string_from_format("%s:%s", name, tag);
  
  pthread_mutex_lock(&g_storage_open_files_dict_mutex);

  t_file_mutex *fm = dictionary_get(g_open_files_dict, key);

  if (fm == NULL) {
    fm = malloc(sizeof(t_file_mutex));
    if (fm == NULL) {
      goto cleanup_unlock_dict;
    }

    if (pthread_rwlock_init(&fm->mutex, NULL) != 0) {
      goto cleanup_free_fm;
    }

    fm->ref_count = 0;
    
    dictionary_put(g_open_files_dict, key, fm);
  }

  fm->ref_count++;
  pthread_mutex_unlock(&g_storage_open_files_dict_mutex);

  for_write ? pthread_rwlock_wrlock(&fm->mutex)
            : pthread_rwlock_rdlock(&fm->mutex);

  free(key);

  return;

cleanup_free_fm:
  free(fm);
cleanup_unlock_dict:
  pthread_mutex_unlock(&g_storage_open_files_dict_mutex);
  free(key);

  return;
}

void unlock_file(const char *name, const char *tag) {
  char *key = string_from_format("%s:%s", name, tag);

  pthread_mutex_lock(&g_storage_open_files_dict_mutex);
  t_file_mutex *fm = dictionary_get(g_open_files_dict, key);

  assert(fm != NULL && "unlock_file() llamado sin lock_file() previo");
  pthread_mutex_unlock(&g_storage_open_files_dict_mutex);

  pthread_rwlock_unlock(&fm->mutex);

  pthread_mutex_lock(&g_storage_open_files_dict_mutex);

  fm->ref_count--;

  if (fm->ref_count == 0) {
    t_file_mutex *removed_fm = dictionary_remove(g_open_files_dict, key);
    pthread_rwlock_destroy(&removed_fm->mutex);
    free(removed_fm);
  }

  pthread_mutex_unlock(&g_storage_open_files_dict_mutex);

  free(key);
}

void cleanup_file_sync(void) {
  void _destroy_file_mutex(char *_, void *value) {
    t_file_mutex *fm = (t_file_mutex *)value;
    pthread_rwlock_destroy(&fm->mutex);
    free(fm);
  }

  if (g_open_files_dict != NULL) {
    dictionary_iterator(g_open_files_dict, _destroy_file_mutex);
    dictionary_destroy(g_open_files_dict);
  }
}
