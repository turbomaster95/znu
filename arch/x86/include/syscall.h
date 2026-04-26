#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>

typedef struct {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
} cpu_context_t;

void syscall_init(void);
void gs_init(uintptr_t stack_top);
void enable_syscalls(void);

#endif
