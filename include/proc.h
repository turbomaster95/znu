#ifndef _PROC_H
#define _PROC_H

#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint64_t pid;
    uint64_t* pml4;      // Private address space
    uintptr_t entry;     // ELF Entry point
    uintptr_t stack_top; // User stack
    bool running;
} process_t;

extern process_t* init_process;

#endif
