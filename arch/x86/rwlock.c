#include <sync.h>

void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    rw->pending_writers = 0;
    spinlock_init(&rw->lock);
    rw->writer_wait = NULL;
    rw->reader_wait = NULL;
}

void rwlock_read_lock(rwlock_t *rw) {
    while (1) {
        uint64_t flags = spinlock_irq_save(&rw->lock);
        
        if (rw->readers >= 0 && rw->pending_writers == 0) {
            rw->readers++;
            spinlock_irq_restore(&rw->lock, flags);
            return;
        }
        
        spinlock_irq_restore(&rw->lock, flags);
        __asm__ volatile ("sti; hlt; cli");
    }
}

void rwlock_read_unlock(rwlock_t *rw) {
    uint64_t flags = spinlock_irq_save(&rw->lock);
    rw->readers--;
    
    if (rw->readers == 0 && rw->writer_wait) {
        /* Wake one writer */
    }
    
    spinlock_irq_restore(&rw->lock, flags);
}

void rwlock_write_lock(rwlock_t *rw) {
    while (1) {
        uint64_t flags = spinlock_irq_save(&rw->lock);
        rw->pending_writers++;
        
        if (rw->readers == 0) {
            rw->readers = -1;
            rw->pending_writers--;
            spinlock_irq_restore(&rw->lock, flags);
            return;
        }
        
        spinlock_irq_restore(&rw->lock, flags);
        __asm__ volatile ("sti; hlt; cli");
    }
}

void rwlock_write_unlock(rwlock_t *rw) {
    uint64_t flags = spinlock_irq_save(&rw->lock);
    rw->readers = 0;
    
    spinlock_irq_restore(&rw->lock, flags);
}
