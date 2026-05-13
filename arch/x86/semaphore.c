#include <sync.h>

void semaphore_init(semaphore_t *sem, int32_t initial) {
    sem->count = initial;
    sem->wait_head = NULL;
    sem->wait_tail = NULL;
    spinlock_init(&sem->lock);
}

void semaphore_wait(semaphore_t *sem) {
    if (!current_process) {
        while (1) {
            uint64_t flags = spinlock_irq_save(&sem->lock);
            if (sem->count > 0) {
                sem->count--;
                spinlock_irq_restore(&sem->lock, flags);
                return;
            }
            spinlock_irq_restore(&sem->lock, flags);
            __asm__ volatile ("pause");
        }
    }
    
    while (1) {
        uint64_t flags = spinlock_irq_save(&sem->lock);
        
        if (sem->count > 0) {
            sem->count--;
            spinlock_irq_restore(&sem->lock, flags);
            return;
        }
        
        current_process->state = TASK_WAITING;
        spinlock_irq_restore(&sem->lock, flags);
        sleep(1);
    }
}


void semaphore_signal(semaphore_t *sem) {
    uint64_t flags = spinlock_irq_save(&sem->lock);
    sem->count++;
    spinlock_irq_restore(&sem->lock, flags);
}

bool semaphore_try_wait(semaphore_t *sem) {
    uint64_t flags = spinlock_irq_save(&sem->lock);
    if (sem->count > 0) {
        sem->count--;
        spinlock_irq_restore(&sem->lock, flags);
        return true;
    }
    spinlock_irq_restore(&sem->lock, flags);
    return false;
}
