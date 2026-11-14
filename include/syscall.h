#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>
#include <arch/syscall.h>

typedef struct {
    char name[128];
    uint32_t type;
    uint32_t size;
} znu_dirent_t;

void syscall_init(void);
void enable_syscalls(void);

#endif
