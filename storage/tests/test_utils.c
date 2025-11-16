#include "test_utils.h"

int create_test_directory(void) {
    cleanup_test_directory();

    if (mkdir(TEST_MOUNT_POINT, 0755) != 0) {
        return -1;
    }

    return 0;
}

int cleanup_test_directory(void) {
    char command[PATH_MAX + 10];
    snprintf(command, sizeof(command), "rm -rf %s", TEST_MOUNT_POINT);

    if (system(command) != 0) {
        return -1;
    }

    return 0;
}

t_log* create_test_logger(void) {
    return log_create("/tmp/test_storage.log", "TEST_STORAGE", false, LOG_LEVEL_DEBUG);
}

void destroy_test_logger(t_log* logger) {
    if (logger != NULL) {
        log_destroy(logger);
    }
}

int file_exists(const char* file_path) {
    struct stat st;
    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
        return 1;
    }
    return 0;
}

int directory_exists(const char* dir_path) {
    struct stat st;
    if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }
    return 0;
}

int read_file_contents(const char* file_path, char* buffer, size_t max_size) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, max_size - 1, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return (int)bytes_read;
}

int is_directory_empty(const char* dir_path) {
    return count_files_in_directory(dir_path) == 0 ? 1 : 0;
}

int count_files_in_directory(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        return -1;
    }

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

int verify_file_size(const char* file_path, size_t expected_size) {
    struct stat st;
    if (stat(file_path, &st) != 0) {
        return -1;
    }

    return (st.st_size == (off_t)expected_size) ? 1 : 0;
}

int files_are_hardlinked(const char* file1_path, const char* file2_path) {
    struct stat st1, st2;

    if (stat(file1_path, &st1) != 0 || stat(file2_path, &st2) != 0) {
        return -1;
    }

    return (st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev) ? 1 : 0;
}

int create_test_superblock(const char* mount_point) {
    char superblock_path[PATH_MAX];
    snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config", mount_point);

    FILE* superblock_file = fopen(superblock_path, "w");
    if (superblock_file == NULL) {
        return -1;
    }

    fprintf(superblock_file, "FS_SIZE=%d\nBLOCK_SIZE=%d\n", TEST_FS_SIZE, TEST_BLOCK_SIZE);
    fclose(superblock_file);

    return 0;
}

int create_test_storage_config(
    char *storage_ip, 
    char *storage_port, 
    char *fresh_start,
    char *mount_point,
    int operation_delay,
    int block_access_delay,
    char *log_level
) {
    if (!storage_ip || !storage_port || !fresh_start || !mount_point || !log_level)
        return -1;

    char storage_config_path[PATH_MAX];
    snprintf(storage_config_path, sizeof(storage_config_path), "%s/storage.config", mount_point);

    FILE* storage_config_file = fopen(storage_config_path, "w");
    if (storage_config_file == NULL)
        return -2;

    fprintf(storage_config_file, 
        "STORAGE_IP=%s\n"
        "STORAGE_PORT=%s\n"
        "FRESH_START=%s\n"
        "MOUNT_POINT=%s\n"
        "OPERATION_DELAY=%d\n"
        "BLOCK_ACCESS_DELAY=%d\n"
        "LOG_LEVEL=%s\n",        
        storage_ip, storage_port, fresh_start, mount_point, 
        operation_delay, block_access_delay, log_level);
    fclose(storage_config_file);

    return 0;
}

int init_logical_blocks(const char *name, const char *tag, int numb_blocks, const char *mount_point) {
    char logical_block_dir[PATH_MAX];
    snprintf(logical_block_dir, PATH_MAX, "%s/files/%s/%s/logical_blocks", mount_point, name, tag);
    
    if(create_dir_recursive(logical_block_dir) < 0)
        return -1;

    char source_path[PATH_MAX];
    snprintf(source_path, PATH_MAX, "%s/physical_blocks/block0000.dat", mount_point);
    for (int i = 0; i < numb_blocks; i++)
    {
        char logical_block_path[PATH_MAX];
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(logical_block_path, PATH_MAX, "%s/%04d.dat", logical_block_dir, i);
        #pragma GCC diagnostic pop

        if (link(source_path, logical_block_path) != 0) {
            return -2;
        }
    }

    return 0;
}

int create_test_metadata(const char *name, const char *tag, int numb_blocks, char *blocks_array_str, char *status, char *mount_point) {
    int size = numb_blocks * TEST_BLOCK_SIZE;

    int total_blocks = TEST_FS_SIZE / TEST_BLOCK_SIZE;
    if (total_blocks < numb_blocks)
        return -1;
    
    char **blocks_array = string_get_string_as_array(blocks_array_str);
    if(string_array_size(blocks_array) != numb_blocks)
        return -2;

    char metadata_path[PATH_MAX];
    snprintf(metadata_path, sizeof(metadata_path), "%s/files/%s/%s/metadata.config", mount_point, name, tag);
    FILE* metadata_file = fopen(metadata_path, "w");
    if (metadata_file == NULL) {
        return -3;
    }

    fprintf(metadata_file, "SIZE=%d\nBLOCKS=%s\nESTADO=%s\n", size, blocks_array_str, status);
    fclose(metadata_file);

    return 0;
}

bool correct_unlock(const char *name, const char *tag) {
    char *key = string_from_format("%s:%s", name, tag);

    pthread_mutex_lock(&g_storage_open_files_dict_mutex);
    t_file_mutex *fm = dictionary_get(g_open_files_dict, key);

    bool retval = fm == NULL;

    pthread_mutex_unlock(&g_storage_open_files_dict_mutex);
    free(key);

    return retval;
}

int mutex_is_free(pthread_mutex_t *mutex) {
    if (pthread_mutex_trylock(mutex) == 0) {
        pthread_mutex_unlock(mutex);
        return 0;
    }
    return -1;
}

void package_simulate_reception(t_package *package)
{
    if (!package || !package->buffer) {
        return;
    }
    
    package->buffer->size = package->buffer->offset;
    
    package_reset_read_offset(package);
}
