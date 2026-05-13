#include <sync.h>
#include <stdio.h>

void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
    lock->name = "unknown";
    lock->owner_pid = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    while (!atomic_cas(&lock->locked, 0, 1)) {
        while (atomic_load(&lock->locked)) {
            __asm__ volatile ("pause");
        }
    }
    compiler_barrier();
    lock->owner_pid = current_process ? current_process->pid : 0;
}

bool spinlock_try_acquire(spinlock_t *lock) {
    if (!atomic_cas(&lock->locked, 0, 1))
        return false;
    compiler_barrier();
    lock->owner_pid = current_process ? current_process->pid : 0;
    return true;
}

void spinlock_release(spinlock_t *lock) {
    lock->owner_pid = 0;
    compiler_barrier();
    atomic_store(&lock->locked, 0);
}

bool spinlock_is_held(spinlock_t *lock) {
    return atomic_load(&lock->locked) != 0;
}

uint64_t spinlock_irq_save(spinlock_t *lock) {
    uint64_t flags;
    __asm__ volatile (
        "pushfq; pop %0; cli"
        : "=r"(flags)
    );
    spinlock_acquire(lock);
    return flags;
}

void spinlock_irq_restore(spinlock_t *lock, uint64_t flags) {
    spinlock_release(lock);
    __asm__ volatile (
        "push %0; popfq"
        :: "r"(flags)
    );
}
