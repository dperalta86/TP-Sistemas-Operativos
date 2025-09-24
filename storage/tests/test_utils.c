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