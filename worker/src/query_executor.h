#ifndef QUERY_EXECUTOR_H
#define QUERY_EXECUTOR_H

#include "worker.h"

typedef enum
{
    QUERY_RESULT_OK,
    QUERY_RESULT_END,
    QUERY_RESULT_EJECT,
    QUERY_RESULT_ERROR,
    QUERY_RESULT_NO_QUERY
} query_result_t;

void *query_executor_thread(void *arg);

#endif
