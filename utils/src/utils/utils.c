#include "utils.h"


t_log* create_logger(char *directory, char *process_name, bool is_active_console, char* s_level) {
    char path_log[PATH_MAX];

    t_log_level level = log_level_from_string(s_level);
    snprintf(path_log, PATH_MAX, "%s/%s.log", directory, process_name);

    t_log* logger = log_create(path_log, process_name, is_active_console, level);

    return logger;
}