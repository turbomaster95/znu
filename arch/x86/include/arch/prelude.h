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

#include <kernel/arch-fulfills.h>

#endif
