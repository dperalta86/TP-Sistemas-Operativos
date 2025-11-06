#include "query_control_manager.h"
#include "worker_manager.h"
#include "init_master.h"
#include "aging.h"
#include <unistd.h>

static int search_worker_id = -1;

void *aging_thread_func(void *arg) {
    t_master *master = (t_master*) arg;

    while (master->running) {
        // Definir cada cuanto se hace la verificación, un tiempo fijo (100ms, 250ms)
        // o una fracción del aging interval (10 veces cada intervalo)
        usleep(master->aging_interval * 100); // -> Por ahora, 10 verificaciones por intervalo

        uint64_t now = now_ms_monotonic();

        if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
            log_error(master->logger, "[Aging] Error al lockear query_table_mutex");
            continue;
        }

        // si ready queue está vacia, nada para hacer
        if (list_is_empty(master->queries_table->ready_queue)) {
            pthread_mutex_unlock(&master->queries_table->query_table_mutex);
            continue;
        }

        bool priorities_changed = false;

        int ready_count = list_size(master->queries_table->ready_queue);
        for (int i = 0; i < ready_count; i++) {
            t_query_control_block *qcb = list_get(master->queries_table->ready_queue, i);
            if (!qcb) continue;

            // Asegurarse que esté realmente en READY (por si hay inconsistencias)
            if (qcb->state != QUERY_STATE_READY) continue;

            // Si no tiene timestamp válido (legacy), inicializarlo
            if (qcb->ready_timestamp == 0) {
                qcb->ready_timestamp = now;
                continue;
            }

            uint64_t elapsed = now - qcb->ready_timestamp; // Calculo cuanto tiempo estuvo en READY

            if (elapsed < (uint64_t)master->aging_interval) {
                // No llegó al intervalo aún
                continue;
            }

            // Esta verificación es por si tenemos tiempo fijo y "se pasa" de un intervalo
            // NO DEBERÍA PASAR...
            int intervals = elapsed / master->aging_interval;
            // Aplicar hasta que prioridad llegue a 0
            int decrements = intervals;
            int original_priority = qcb->priority;

            if (decrements > 0 && qcb->priority > 0) {
                if (decrements >= qcb->priority) {
                    qcb->priority = 0;
                } else {
                    qcb->priority -= decrements;
                }
                priorities_changed = true;

                // Actualizamos en timestamp en Ready
                qcb->ready_timestamp += (uint64_t)intervals * (uint64_t)master->aging_interval;

                log_info(master->logger,
                         "##<QUERY_ID: %d> Cambio de prioridad: <PRIORIDAD_ANTERIOR: %d> - <PRIORIDAD_NUEVA: %d>",
                         qcb->query_id, original_priority, qcb->priority);
            }
        }

        if (priorities_changed) {
            // Reordenar la READY queue
            list_sort(master->queries_table->ready_queue, _qcb_priority_compare);
        }

        pthread_mutex_unlock(&master->queries_table->query_table_mutex);

        check_preemption(master);
    }

    return NULL;
}

void check_preemption(t_master *master) {

    pthread_mutex_lock(&master->workers_table->worker_table_mutex);
    pthread_mutex_lock(&master->queries_table->query_table_mutex);

    // Nada para hacer si no hay running o no hay ready
    if(list_is_empty(master->workers_table->busy_list) ||
       list_is_empty(master->queries_table->ready_queue)) {
        goto unlock_and_exit;
    }

    // La mejor query en READY (ya ordenado por prioridad)
    t_query_control_block *best_ready =
        list_get(master->queries_table->ready_queue, 0);

    // Buscar la query de menor prioridad en RUNNING
    t_query_control_block *worst_running = NULL;

    for(int i = 0; i < list_size(master->queries_table->running_list); i++) {
        t_query_control_block *qcb = list_get(master->queries_table->running_list, i);
        if(worst_running == NULL || qcb->priority > worst_running->priority) {
            worst_running = qcb;
        }
    }

    // Si NO hay preemption necesaria
    if(worst_running == NULL || best_ready->priority >= worst_running->priority) {
        goto unlock_and_exit;
    }

    // Desalojar...
    preempt_query_in_exec(worst_running, master);

unlock_and_exit:
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
}


int preempt_query_in_exec(t_query_control_block *qcb, t_master *master) {
    if (!qcb || !master) return -1;

    search_worker_id = qcb->assigned_worker_id;
    t_worker_control_block *worker = list_find(
        master->workers_table->worker_list,
        match_worker_by_id
    );

    if (!worker || worker->socket_fd <= 0) {
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);

        log_error(master->logger,
            "[preempt_query_in_exec] Worker inválido durante preemption. Finalizando Query ID=%d",
            qcb->query_id
        );

        // Estas dos funciones están en la PR que maneja desconexiones
        // Quedan comentadas hasta que se apruebe
        // finalize_query_with_error(qcb, master, "Error en preemption");
        // cleanup_query_resources(qcb, master);
        return -1;
    }

    // Envío solicitud de desalojo, la respuesta la manejo en el worker_manager
    t_package *pkg = package_create_empty(OP_WORKER_PREEMPT_REQ);
    package_add_uint32(pkg, (uint32_t)qcb->query_id);
    package_send(pkg, worker->socket_fd);
    package_destroy(pkg);

    log_info(master->logger,
        "## Se desaloja la Query id: %d (<PRIORIDAD: %d>) del Worker <WORKER_ID: %d> - Motivo: <PRIORIDAD>",
        qcb->query_id, qcb->priority, worker->worker_id
    );

    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
    return 0;
}

bool _qcb_priority_compare(void *a, void *b) {
    t_query_control_block *q1 = a;
    t_query_control_block *q2 = b;
    return q1->priority < q2->priority; // menor número gana
}

bool match_worker_by_id(void *elem) {
    t_worker_control_block *w = elem;
    return w && w->worker_id == search_worker_id;
}