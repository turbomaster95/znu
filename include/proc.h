#ifndef _PROC_H
#define _PROC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <idt.h>
#include <vfs.h>
#include <vfse.h>

#define SIGINT   2
#define SIGKILL  9
#define SIGTERM 15

typedef void (*sig_handler_t)(int);

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_WAITING,
    TASK_ZOMBIE
} task_state_t;

typedef enum {
    PRIO_IDLE = 0,
    PRIO_LOW,
    PRIO_NORMAL,
    PRIO_HIGH
} task_prio_t;

typedef enum {
    WAIT_NONE,
    WAIT_CHILD,
    WAIT_TTY
} process_wait_reason_t;

#define MAX_PROCESSES 64
#define MAX_FILES 32
#define KTHREAD_STACK_SIZE 16384

typedef struct process {
    uint64_t pid;
    uint64_t parent_pid;
    int exit_code;

    uint64_t* pml4;

    uintptr_t entry;
    uintptr_t stack_top;
    uintptr_t kstack_top;

    uintptr_t brk;
    uintptr_t brk_start;

    registers_t  context;
    registers_t* context_ptr;

    task_state_t state;
    task_prio_t priority;

    process_wait_reason_t wait_reason;
    void *wait_channel;
    int wait_pid;

    vfs_file_t* files[MAX_FILES];
    vfse_process_t* vfse;

    uint8_t sse_state[512] __attribute__((aligned(16)));

    uintptr_t tls_base;
    uint32_t tls_size;

    bool is_kthread;
    uint64_t sleep_deadline;

    uint64_t pending_signals;
    uint64_t blocked_signals;
    uintptr_t signal_handlers[64];

    registers_t saved_user_context;
    bool inside_signal_handler;

    char name[32];
} process_t;

extern process_t* processes[MAX_PROCESSES];
extern process_t* current_process;
extern process_t* init_process;
extern process_t* idle_process;
extern uint64_t next_pid;
extern int process_count;

registers_t* scheduler(registers_t* regs);
void init_scheduler(void);
int add_process(process_t* proc);

int do_wait(int pid, int* status, bool* should_block);
registers_t* do_exit(int code);
process_t* clone_process(process_t* src, registers_t* regs);

void kernel_signal_raise(process_t* proc, int signum);
void signal_check_and_deliver(registers_t* regs);

process_t* kthread_create(void (*fn)(void*), void* arg, const char* name);
void kthread_sleep_ms(uint64_t ms);
void kthread_yield(void);
void kthread_exit(int code);
void kthread_wake(process_t* thread);

uint64_t get_timer_ticks(void);

#endif
