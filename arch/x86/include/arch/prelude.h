#ifndef ARCH_PRELUDE
#define ARCH_PRELUDE

#include <cpuid.h>
#include <stdint.h>
#include <stdbool.h>

static inline void stac(void) {
    if (cpu_has(SMAP_SUPPORT)) {
        asm volatile("stac" ::: "cc");
    } else {
        // do nothing here bc it's probs a old machine or qemu without kvm or any accel lol
    }
}

static inline void clac(void) {
    if (cpu_has(SMAP_SUPPORT)) {
        asm volatile("clac" ::: "cc");
    } else {
        // do nothing here bc it's probs a old machine or qemu without kvm or any accel lol
    }
}

#define _uaccess_begin()  stac()
#define _uaccess_end()    clac()

void arch_early_init(void);
void arch_interrupt_init(void);
void arch_timer_init(uint32_t frequency);
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);
_Noreturn void arch_halt(void);
void arch_debug_cpu_info(void);

#endif
