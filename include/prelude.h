#ifndef _PRELUDE_H
#define _PRELUDE_H

#include "../arch/x86/include/cpuid.h"

#define PERFORM __attribute__((no_sanitize("undefined", "address", "thread"), noinline))

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

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

#endif
