#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>

#define SYS_ARCH_PRCTL     158
#define ARCH_SET_FS        0x1002
#define MSR_FS_BASE        0xC0000100

typedef struct {
    char name[128];
    uint32_t type;
    uint32_t size;
} znu_dirent_t;

typedef struct {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
    uint64_t user_rdx_scratch;
} cpu_context_t;

void syscall_init(void);
void gs_init(uintptr_t stack_top);
void enable_syscalls(void);

#endif
