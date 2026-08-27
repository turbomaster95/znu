#include <proc.h>
#include <string.h>
#include <idt.h>

void kernel_signal_raise(process_t* proc, int signum) {
    if (!proc || signum <= 0 || signum >= 64) return;

    proc->pending_signals |= (1ULL << signum);

    if (proc->state == TASK_WAITING || proc->state == TASK_SLEEPING) {
        proc->state = TASK_READY;
    }
}

void signal_check_and_deliver(registers_t* regs) {
    if (!current_process || current_process->inside_signal_handler) return;

    uint64_t actionable = current_process->pending_signals & ~current_process->blocked_signals;
    if (!actionable) return;

    int signum = 0;
    for (int i = 1; i < 64; i++) {
        if (actionable & (1ULL << i)) {
            signum = i;
            break;
        }
    }

    current_process->pending_signals &= ~(1ULL << signum);

    uintptr_t handler = current_process->signal_handlers[signum];

    if (handler == 0) {
        if (signum == SIGINT || signum == SIGKILL || signum == SIGTERM) {
            do_exit(signum);
            return;
        }
        return; 
    }

    current_process->inside_signal_handler = true;

    memcpy(&current_process->saved_user_context, regs, sizeof(registers_t));

    regs->rip = handler;

    regs->rdi = (uint64_t)signum;

    regs->rax = 0;
}
