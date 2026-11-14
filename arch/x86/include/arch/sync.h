#ifndef _SYNC_H
#define _SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <lapic.h>

extern uint32_t lapic_ticks_per_ms;

static inline uint32_t atomic_fetch_add(volatile uint32_t *ptr, uint32_t val) {
    uint32_t ret;
    __asm__ volatile ("lock; xadd %0, %1" : "=r"(ret), "+m"(*ptr) : "0"(val) : "memory");
    return ret;
}

static inline uint32_t atomic_add(volatile uint32_t *ptr, uint32_t val) {
    return atomic_fetch_add(ptr, val) + val;
}

static inline uint32_t atomic_fetch_sub(volatile uint32_t *ptr, uint32_t val) {
    return atomic_fetch_add(ptr, -(int32_t)val);
}

static inline bool atomic_cas(volatile uint32_t *ptr, uint32_t expected, uint32_t desired) {
    uint32_t prev;
    __asm__ volatile ("lock; cmpxchg %2, %1" : "=a"(prev), "+m"(*ptr) : "r"(desired), "0"(expected) : "memory");
    return prev == expected;
}

static inline void atomic_store(volatile uint32_t *ptr, uint32_t val) {
    __asm__ volatile ("mov %1, %0" : "=m"(*ptr) : "r"(val) : "memory");
}

static inline uint32_t atomic_load(volatile uint32_t *ptr) {
    return *ptr;
}

#endif
