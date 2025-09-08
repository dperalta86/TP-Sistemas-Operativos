#include "logger.h"

#include <unistd.h>
#include <limits.h>
#include <stdio.h>

static t_log *global_logger = NULL;

int logger_init(const char *process_name, const char *level_str, bool to_console)
{
    if (global_logger) return 0;

    char current_working_directory[PATH_MAX];
    char log_path[PATH_MAX];

    if (getcwd(current_working_directory, sizeof(current_working_directory)) == NULL)
    {
        return -1;
    }

    t_log_level level = log_level_from_string((char *)level_str);

    snprintf(log_path, sizeof(log_path), "%s/worker.log", current_working_directory);

    global_logger = log_create(log_path, process_name, to_console, level);
    if (!global_logger)
        return -1;

    return 0;
}

t_log *logger_get(void)
{
    return global_logger;
}

void logger_destroy(void)
{
    if (global_logger)
    {
        log_destroy(global_logger);
        global_logger = NULL;
    }
}
