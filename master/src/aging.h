#ifndef AGING_H
#define AGING_H


bool _qcb_priority_compare(void *a, void *b);
bool match_worker_by_id(void *elem);
void *aging_thread_func(void *arg);
void check_preemption(t_master *master);
int preempt_query_in_exec(t_query_control_block *qcb, t_master *master);

#endif