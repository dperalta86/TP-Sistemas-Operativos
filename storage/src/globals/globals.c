#include "globals.h"

t_storage_config* g_storage_config;
t_log* g_storage_logger;
int g_worker_counter = 0;

// semáforos
pthread_mutex_t g_worker_counter_mutex = PTHREAD_MUTEX_INITIALIZER;