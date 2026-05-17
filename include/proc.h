#ifndef _PROC_H
#define _PROC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <idt.h>

#define SIGINT   2
#define SIGKILL  9
#define SIGTERM  15

typedef void (*sig_handler_t)(int);

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_WAITING,
    TASK_ZOMBIE
} task_state_t;

#include <vfs.h>

#define MAX_FILES 32

typedef struct {
    uint64_t pid;
    uint64_t parent_pid;
    int exit_code;
    uint64_t* pml4;      // Private address space
    uintptr_t entry;     // ELF Entry point
    uintptr_t stack_top; // User stack
    uintptr_t kstack_top; // Kernel stack top
    uintptr_t brk;       // Current heap end
    uintptr_t brk_start; // Original heap start
    registers_t* context_ptr; // Saved registers on the kernel stack
    task_state_t state;
    vfs_file_t* files[MAX_FILES];
    uint8_t sse_state[512] __attribute__((aligned(16)));

    // --- SIGNAL SUBSYSTEM EXTENSIONS ---
    uint64_t pending_signals;    // Bitmask tracking pending signals (1-64)
    uint64_t blocked_signals;    // Masked signals
    uintptr_t signal_handlers[64]; // User-space handler addresses
    
    registers_t saved_user_context; // Snapshot of registers before hijacking
    bool inside_signal_handler;    // Deadlock/nested execution guard
} process_t;

extern process_t* current_process;
extern process_t* init_process;

registers_t* scheduler(registers_t* regs);
void init_scheduler(void);
void add_process(process_t* proc);

// Signal Core Control APIs
void kernel_signal_raise(process_t* proc, int signum);
void signal_check_and_deliver(registers_t* regs);

#endif
