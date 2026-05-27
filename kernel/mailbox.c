#include <mailbox.h>
#include <lapic.h>
#include <stddef.h>
#include <gdt.h>

static struct ap_mailbox mailboxes[MAX_CPUS];

void mailbox_init(void) {
    for (int i = 0; i < MAX_CPUS; i++) {
        mailboxes[i].func = NULL;
        mailboxes[i].arg = NULL;
        mailboxes[i].task_pending = false;
    }
}

void mailbox_send_task(int cpu_id, ap_task_func func, void *arg) {
    if (cpu_id >= MAX_CPUS) return;

    mailboxes[cpu_id].func = func;
    mailboxes[cpu_id].arg = arg;

    __atomic_store_n(&mailboxes[cpu_id].task_pending, true, __ATOMIC_RELEASE);

    lapic_send_ipi(cpu_id, IPI_WAKEUP_VECTOR);
}

void mailbox_handle_task(int cpu_id) {
    if (cpu_id >= MAX_CPUS) return;

    if (__atomic_load_n(&mailboxes[cpu_id].task_pending, __ATOMIC_ACQUIRE)) {
        if (mailboxes[cpu_id].func != NULL) {
            mailboxes[cpu_id].func(mailboxes[cpu_id].arg);
        }

        __atomic_store_n(&mailboxes[cpu_id].task_pending, false, __ATOMIC_RELEASE);
    }
}
