#include "utils.h"


t_log* create_logger(char *directory, char *process_name, bool is_active_console, t_log_level log_level) {
    char path_log[PATH_MAX];

    snprintf(path_log, PATH_MAX, "%s/%s.log", directory, process_name);

    t_log* logger = log_create(path_log, process_name, is_active_console, log_level);

    return logger;
}
