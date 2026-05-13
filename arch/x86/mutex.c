#include <sync.h>
#include <proc.h>
#include <stdlib.h>

static void __wait_enqueue(mutex_t *mutex, mutex_waiter_t *waiter) {
    waiter->proc = current_process;
    waiter->next = NULL;
    
    if (mutex->wait_tail) {
        mutex->wait_tail->next = waiter;
    } else {
        mutex->wait_head = waiter;
    }
    mutex->wait_tail = waiter;
    mutex->wait_count++;
}

static process_t* __wait_dequeue(mutex_t *mutex) {
    if (!mutex->wait_head)
        return NULL;
    
    mutex_waiter_t *w = mutex->wait_head;
    mutex->wait_head = w->next;
    if (!mutex->wait_head)
        mutex->wait_tail = NULL;
    mutex->wait_count--;
    
    process_t *proc = w->proc;
    return proc;
}

static void __wait_remove(mutex_t *mutex, process_t *proc) {
    mutex_waiter_t **pp = &mutex->wait_head;
    while (*pp) {
        if ((*pp)->proc == proc) {
            mutex_waiter_t *w = *pp;
            *pp = w->next;
            if (!*pp) mutex->wait_tail = NULL;
            mutex->wait_count--;
            return;
        }
        pp = &(*pp)->next;
    }
}

void mutex_init(mutex_t *mutex) {
    mutex->state = 0;
    mutex->owner_pid = 0;
    mutex->recursion = 0;
    mutex->wait_head = NULL;
    mutex->wait_tail = NULL;
    mutex->wait_count = 0;
    spinlock_init(&mutex->wait_lock);
}

int mutex_lock(mutex_t *mutex) {
    if (!current_process) {
        while (!atomic_cas(&mutex->state, 0, 1)) {
            __asm__ volatile ("pause");
        }
        compiler_barrier();
        return 0;
    }
    
    uint64_t my_pid = current_process->pid;
    
    /* Fast path: uncontended */
    if (atomic_cas(&mutex->state, 0, 1)) {
        mutex->owner_pid = my_pid;
        return 0;
    }
    
    /* Slow path: contended */
    atomic_add(&mutex->state, 1);
    mutex_waiter_t waiter;
    
    while (1) {
        spinlock_acquire(&mutex->wait_lock);
        __wait_enqueue(mutex, &waiter);
        current_process->state = TASK_WAITING;
        spinlock_release(&mutex->wait_lock);
        
        sleep(1);
        
        uint32_t expected = 1;
        if (atomic_cas(&mutex->state, expected, 1)) {
            mutex->owner_pid = my_pid;
            return 0;
        }
    }
}

int mutex_trylock(mutex_t *mutex) {
    if (!current_process) {
        if (atomic_cas(&mutex->state, 0, 1)) {
            return 0;
        }
        return -1;
    }
    
    if (atomic_cas(&mutex->state, 0, 1)) {
        mutex->owner_pid = current_process->pid;
        return 0;
    }
    return -1;
}

void mutex_unlock(mutex_t *mutex) {
    if (!current_process) {
        compiler_barrier();
        atomic_store(&mutex->state, 0);
        return;
    }
    
    uint64_t my_pid = current_process->pid;
    
    if (mutex->owner_pid != my_pid) {
        debugln("[mutex] ERROR: PID %lu unlocking mutex owned by %lu!\n", 
               my_pid, mutex->owner_pid);
        return;
    }
    
    mutex->owner_pid = 0;
    compiler_barrier();
    
    if (atomic_cas(&mutex->state, 1, 0)) {
        return;
    }
    
    atomic_fetch_sub(&mutex->state, 1);
    
    spinlock_acquire(&mutex->wait_lock);
    process_t *woken = __wait_dequeue(mutex);
    if (woken) {
        woken->state = TASK_READY;
    }
    spinlock_release(&mutex->wait_lock);
}

int mutex_lock_timeout(mutex_t *mutex, uint32_t timeout_ms) {
    if (!current_process) {
        while (!atomic_cas(&mutex->state, 0, 1)) {
            __asm__ volatile ("pause");
        }
        compiler_barrier();
        return 0;
    }
    
    uint64_t my_pid = current_process->pid;
    
    if (atomic_cas(&mutex->state, 0, 1)) {
        mutex->owner_pid = my_pid;
        return 0;
    }
    
    atomic_add(&mutex->state, 1);
    mutex_waiter_t waiter;
    
    uint64_t deadline = lapic_ticks_per_ms + timeout_ms;
    
    while (1) {
        spinlock_acquire(&mutex->wait_lock);
        __wait_enqueue(mutex, &waiter);
        current_process->state = TASK_WAITING;
        spinlock_release(&mutex->wait_lock);
        
        sleep(1);
        
        if (lapic_ticks_per_ms >= deadline) {
            spinlock_acquire(&mutex->wait_lock);
            __wait_remove(mutex, current_process);
            spinlock_release(&mutex->wait_lock);
            atomic_fetch_sub(&mutex->state, 1);
            return -2;
        }
        
        uint32_t expected = 1;
        if (atomic_cas(&mutex->state, expected, 1)) {
            mutex->owner_pid = my_pid;
            return 0;
        }
    }
}
