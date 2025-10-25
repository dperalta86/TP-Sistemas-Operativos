#include "globals.h"

t_storage_config *g_storage_config;
t_log *g_storage_logger;
int g_worker_counter = 0;
t_dictionary *g_open_files_dict = NULL;

// semáforos
pthread_mutex_t g_worker_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_storage_bitmap_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_storage_open_files_dict_mutex = PTHREAD_MUTEX_INITIALIZER;
