// kernel/signal.c
#include <proc.h>
#include <string.h>
#include <idt.h>

extern void do_exit(int code);

void kernel_signal_raise(process_t* proc, int signum) {
    if (!proc || signum <= 0 || signum >= 64) return;

    proc->pending_signals |= (1ULL << signum);

    // Kick the task back to the execution queue if it was blocking
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

    // Dequeue signal
    current_process->pending_signals &= ~(1ULL << signum);

    uintptr_t handler = current_process->signal_handlers[signum];

    // 1. Default Action Handling
    if (handler == 0) {
        if (signum == SIGINT || signum == SIGKILL || signum == SIGTERM) {
            do_exit(signum); // Core termination sequence
            return;
        }
        return; 
    }

    // 2. Custom User-space Interception
    current_process->inside_signal_handler = true;

    // Capture register footprint state safely
    memcpy(&current_process->saved_user_context, regs, sizeof(registers_t));

    // Force execution frame to drop right onto user's registered code space
    regs->rip = handler;

    // System V ABI: First parameter passed to function is handled inside RDI
    regs->rdi = (uint64_t)signum;

    // Clear return accumulators
    regs->rax = 0;
}
