#ifndef _PROC_H
#define _PROC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <idt.h>

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
    registers_t context; // Saved registers
    task_state_t state;
    vfs_file_t* files[MAX_FILES];
} process_t;

extern process_t* current_process;
extern process_t* init_process;

void scheduler(registers_t* regs);
void init_scheduler(void);
void add_process(process_t* proc);

#endif
