#ifndef ARCH_SYSCALL_H
#define ARCH_SYSCALL_H

#include <stdint.h>

#define SYS_ARCH_PRCTL     158
#define ARCH_SET_FS        0x1002
#define MSR_FS_BASE        0xC0000100

typedef struct {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
    uint64_t user_rdx_scratch;
} cpu_context_t;

void x86_64_syscall_init(void);
void x86_64_gs_init(uintptr_t stack_top);

#endif
