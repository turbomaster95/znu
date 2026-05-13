#ifndef _PRELUDE_H
#define _PRELUDE_H

#define PERFORM __attribute__((no_sanitize("undefined", "address", "thread"), noinline))

static inline void stac(void) {
    asm volatile("stac" ::: "cc");
}

static inline void clac(void) {
    asm volatile("clac" ::: "cc");
}

#endif
