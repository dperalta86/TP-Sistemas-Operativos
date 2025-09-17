#include "logger.h"

#include <unistd.h>
#include <limits.h>
#include <stdio.h>

static t_log *global_logger = NULL;

int logger_init(const char *process_name, t_log_level log_level, bool to_console)
{
    if (global_logger) return 0;

    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        return -1;
    }

    char *log_path = NULL;
    if (asprintf(&log_path, "%s/%s.log", cwd, process_name) == -1) {
        free(cwd);
        return -1;
    }

    global_logger = log_create(log_path, process_name, to_console, log_level);

    free(cwd);
    free(log_path);

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
