#ifndef ARCH_SYNC_H
#define ARCH_SYNC_H

#include <stdint.h>
#include <stdbool.h>

extern uint32_t lapic_ticks_per_ms;
void sleep(uint32_t ms);

static inline void arch_cpu_relax(void) {
    arch_yield();
}

static inline uint32_t atomic_fetch_add(volatile uint32_t *ptr, uint32_t val) {
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_add(volatile uint32_t *ptr, uint32_t val) {
    return __atomic_add_fetch(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_fetch_sub(volatile uint32_t *ptr, uint32_t val) {
    return __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST);
}

static inline bool atomic_cas(volatile uint32_t *ptr, uint32_t expected, uint32_t desired) {
    return __atomic_compare_exchange_n(ptr, &expected, desired, false,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline void atomic_store(volatile uint32_t *ptr, uint32_t val) {
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_load(volatile uint32_t *ptr) {
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

#endif
