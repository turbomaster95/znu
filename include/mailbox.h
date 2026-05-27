#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*ap_task_func)(void *);

struct ap_mailbox {
    ap_task_func func;
    void *arg;
    volatile bool task_pending;
};

#define IPI_WAKEUP_VECTOR 0x40

void mailbox_init(void);
void mailbox_send_task(int cpu_id, ap_task_func func, void *arg);
void mailbox_handle_task(int cpu_id);

#endif
