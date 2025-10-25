#include "file_locks.h"
#include "globals/globals.h"
#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <stdlib.h>

void lock_file(const char *name, const char *tag) {
  char *key = string_from_format("%s:%s", name, tag);

  pthread_mutex_lock(&g_storage_open_files_dict_mutex);
  t_file_mutex *fm = dictionary_get(g_open_files_dict, key);

  if (fm == NULL) {
    fm = malloc(sizeof(t_file_mutex));
    pthread_mutex_init(&fm->mutex, NULL);
    fm->ref_count = 0;
    dictionary_put(g_open_files_dict, key, fm);
  }

  fm->ref_count++;

  pthread_mutex_unlock(&g_storage_open_files_dict_mutex);

  free(key);

  pthread_mutex_lock(&fm->mutex);
}

void unlock_file(const char *name, const char *tag) {
  char *key = string_from_format("%s:%s", name, tag);

  pthread_mutex_lock(&g_storage_open_files_dict_mutex);
  t_file_mutex *fm = dictionary_get(g_open_files_dict, key);

  if (fm != NULL) {
    pthread_mutex_unlock(&fm->mutex);
    fm->ref_count--;
    if (fm->ref_count == 0) {
      dictionary_remove(g_open_files_dict, key);
      pthread_mutex_destroy(&fm->mutex);
      free(fm);
    }
  }

  pthread_mutex_unlock(&g_storage_open_files_dict_mutex);

  free(key);
}

void cleanup_file_sync(void) {
  void _destroy_file_mutex(char *_, void *value) {
    t_file_mutex *fm = (t_file_mutex *)value;
    pthread_mutex_destroy(&fm->mutex);
    free(fm);
  }

  if (g_open_files_dict != NULL) {
    dictionary_iterator(g_open_files_dict, _destroy_file_mutex);
    dictionary_destroy(g_open_files_dict);
  }
}
