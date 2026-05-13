#ifndef _SYNC_H
#define _SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <proc.h>
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

#define compiler_barrier() __asm__ volatile ("" ::: "memory")

typedef struct {
    volatile uint32_t locked;
    const char *name;
    uint64_t owner_pid;
} spinlock_t;

#define SPINLOCK_INIT { .locked = 0 }

void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);
bool spinlock_try_acquire(spinlock_t *lock);
bool spinlock_is_held(spinlock_t *lock);

uint64_t spinlock_irq_save(spinlock_t *lock);
void spinlock_irq_restore(spinlock_t *lock, uint64_t flags);

typedef struct mutex_waiter {
    struct mutex_waiter *next;
    process_t *proc;
} mutex_waiter_t;

typedef struct {
    volatile uint32_t state;        /* 0=free, 1=locked, >1=contended */
    volatile uint64_t owner_pid;
    volatile uint32_t recursion;    /* For recursive mutexes */
    
    mutex_waiter_t *wait_head;
    mutex_waiter_t *wait_tail;
    uint32_t wait_count;
    
    spinlock_t wait_lock;           /* Protects the wait queue */
} mutex_t;

#define MUTEX_INIT { .state = 0, .owner_pid = 0, .recursion = 0, .wait_head = NULL, .wait_tail = NULL, .wait_count = 0, .wait_lock = SPINLOCK_INIT }

void mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
int mutex_lock_timeout(mutex_t *mutex, uint32_t timeout_ms);

typedef struct {
    volatile int32_t readers;       /* -1 = writer active, >0 = readers active */
    volatile uint32_t pending_writers;
    spinlock_t lock;
    mutex_waiter_t *writer_wait;
    mutex_waiter_t *reader_wait;
} rwlock_t;

#define RWLOCK_INIT { .readers = 0, .pending_writers = 0, .lock = SPINLOCK_INIT, .writer_wait = NULL, .reader_wait = NULL }

void rwlock_init(rwlock_t *rw);
void rwlock_read_lock(rwlock_t *rw);
void rwlock_read_unlock(rwlock_t *rw);
void rwlock_write_lock(rwlock_t *rw);
void rwlock_write_unlock(rwlock_t *rw);

typedef struct {
    volatile int32_t count;
    mutex_waiter_t *wait_head;
    mutex_waiter_t *wait_tail;
    spinlock_t lock;
} semaphore_t;

#define SEMAPHORE_INIT(n) { .count = n, .wait_head = NULL, .wait_tail = NULL, .lock = SPINLOCK_INIT }

void semaphore_init(semaphore_t *sem, int32_t initial);
void semaphore_wait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);
bool semaphore_try_wait(semaphore_t *sem);

#endif
