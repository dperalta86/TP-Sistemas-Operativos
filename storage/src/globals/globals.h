#ifndef STORAGE_GLOBALS_H_
#define STORAGE_GLOBALS_H_

#include <pthread.h>
#include <commons/log.h>

typedef struct {
    char* storage_ip;
    char* storage_port;
    bool fresh_start;
    char* mount_point;
    int operation_delay;
    int block_access_delay;
    int fs_size;
    int block_size;
    t_log_level log_level;
} t_storage_config;

typedef struct {
    int client_socket;
    char *client_id;
} t_client_data;

extern t_log* g_storage_logger;
extern t_storage_config* g_storage_config;
extern int g_worker_counter;

// semáforos
extern pthread_mutex_t g_worker_counter_mutex;

#endif