#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>

void syscall_init(void);
void gs_init(uintptr_t stack_top);

#endif
