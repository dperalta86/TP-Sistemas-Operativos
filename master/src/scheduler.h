#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "init_master.h"
#include "worker_manager.h"
#include "query_control_manager.h"

/**
 * Intenta despachar una query READY a un worker IDLE.
 * Retorna 0 si se despachó correctamente, -1 en caso de error.
 * Si no hay workers IDLE o queries READY, retorna 0 sin hacer nada.
 */
int try_dispatch(t_master* master);

int send_query_to_worker(t_master *master, t_worker_control_block *worker, t_query_control_block *query);

#endif // SCHEDULER_H