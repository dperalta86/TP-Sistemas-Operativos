#include "utils.h"


t_log* create_logger(char *directory, char *process_name, bool is_active_console, t_log_level log_level) {
    char path_log[PATH_MAX];

    snprintf(path_log, PATH_MAX, "%s/%s.log", directory, process_name);

    t_log* logger = log_create(path_log, process_name, is_active_console, log_level);

    return logger;
}

size_t strlcpy(char *dest, const char *source, size_t size) {
    if (size == 0) {
        return strlen(source);
    }

    strncpy(dest, source, size - 1);
    dest[size - 1] = '\0';

    return strlen(source);
}

char* get_stringified_array(char** array) {
    char* resultado = string_new();
    string_append(&resultado, "[");
    
    for(int i = 0; array[i] != NULL; i++) {
        if(i > 0) {
            string_append(&resultado, ",");
        }
        string_append(&resultado, array[i]);
    }
    
    string_append(&resultado, "]");
    return resultado;
}

bool regular_file_exists(char *file_path) {
  struct stat info;
  return (stat(file_path, &info) == 0 && S_ISREG(info.st_mode));
}
