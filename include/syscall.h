#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>
#ifdef __KERNEL__
#include <arch/syscall.h>
#endif

typedef struct {
    char name[128];
    uint32_t type;
    uint32_t size;
} znu_dirent_t;

void syscall_init(void);
void enable_syscalls(void);
void gs_init(uintptr_t stack_top);

#endif
