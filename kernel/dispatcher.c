#include <proc.h>
#include <regs.h>
#include <stdbool.h>

#define SCHED_DOMAIN_QUANTUM 4

static unsigned int domain_ticks = 0;

extern bool process_has_runnable(void);
extern bool kthread_has_runnable(void);

registers_t* scheduler_dispatch(registers_t* regs) {
    if (!current_process) {
        if (regs && !init_process)
            return kthread_scheduler(regs);

        if (kthread_has_runnable())
            return kthread_scheduler(regs);

        return process_scheduler(regs);
    }

    bool user_ready = process_has_runnable();
    bool kthread_ready = kthread_has_runnable();

    if (!init_process && !user_ready && !kthread_ready)
        return kthread_scheduler(regs);

    if (current_process->is_kthread) {
        if (user_ready && (!kthread_ready || domain_ticks >= SCHED_DOMAIN_QUANTUM)) {
            domain_ticks = 0;
            return process_scheduler(regs);
        }

        if (kthread_ready) {
            domain_ticks++;
            return kthread_scheduler(regs);
        }

        return process_scheduler(regs);
    }

    if (kthread_ready && (!user_ready || domain_ticks >= SCHED_DOMAIN_QUANTUM)) {
        domain_ticks = 0;
        return kthread_scheduler(regs);
    }

    domain_ticks++;
    return process_scheduler(regs);
}

registers_t* scheduler(registers_t* regs) {
    return process_scheduler(regs);
}
