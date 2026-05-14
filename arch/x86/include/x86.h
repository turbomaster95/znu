#ifndef _X86_H
#define _X86_H

#include <stddef.h>

// This .h file is used for .c files in arch/x86 whom need exporting some functions but dont really need their own whole .h file

void ps2_init(void);
void pat_init(void);

static inline void cpu_pause(void) {
    __asm__ volatile ("pause");
}

static inline void cpu_halt(void) {
    __asm__ volatile ("cli; hlt");
}

static inline uint32_t cpu_id(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ebx >> 24);
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint32_t irq_save_disable(void) {
    uint32_t flags;
    __asm__ volatile (
        "pushf; pop %0; cli"
        : "=r" (flags)
    );
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ volatile (
        "push %0; popf"
        :: "r" (flags)
    );
}

static inline void irq_enable(void) {
    __asm__ volatile ("sti");
}

static inline void irq_disable(void) {
    __asm__ volatile ("cli");
}

#endif
