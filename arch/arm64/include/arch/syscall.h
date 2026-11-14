#ifndef ARCH_SYSCALL_H
#define ARCH_SYSCALL_H

#include <stdint.h>

typedef struct {
    uint64_t kernel_stack;
    uint64_t user_stack;
    uint64_t user_tp;
} cpu_context_t;

#define ARM64_SYS_ARCH_PRCTL 158

static inline uint64_t arm64_read_tpidr_el0(void) {
    uint64_t v;
    __asm__ volatile ("mrs %0, TPIDR_EL0" : "=r"(v));
    return v;
}

static inline void arm64_write_tpidr_el0(uint64_t v) {
    __asm__ volatile ("msr TPIDR_EL0, %0" :: "r"(v) : "memory");
}

void arm64_syscall_init(void);

#endif
