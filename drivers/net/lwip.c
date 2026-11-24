#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include <sync.h>
#include <proc.h>
#include <page.h>
#include <stdlib.h>
#include <timekeeper.h>

u32_t sys_jiffies(void) { return (u32_t)nifties; }
u32_t sys_now(void) { return (u32_t)nifties_to_msecs(nifties); }

static spinlock_t prot_lock = SPINLOCK_INIT;

sys_prot_t sys_arch_protect(void) {
    uint64_t flags = spinlock_irq_save(&prot_lock);
    return (sys_prot_t)flags;
}
void sys_arch_unprotect(sys_prot_t pval) {
    spinlock_irq_restore(&prot_lock, (uint64_t)pval);
}

err_t sys_mutex_new(sys_mutex_t *mutex) {
    if (!mutex) return ERR_MEM;
    mutex_t *m = kmalloc(sizeof(mutex_t));
    if (!m) return ERR_MEM;
    mutex_init(m);
    *mutex = m;
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex)  { mutex_lock(*mutex); }
void sys_mutex_unlock(sys_mutex_t *mutex) { mutex_unlock(*mutex); }
void sys_mutex_free(sys_mutex_t *mutex)  { kfree(*mutex); }

err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    semaphore_t *s = kmalloc(sizeof(semaphore_t));
    semaphore_init(s, count);
    *sem = s;
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem) { semaphore_signal(*sem); }
u32_t sys_sem_trywait(sys_sem_t *sem) { return semaphore_try_wait(*sem) ? 0 : SYS_ARCH_TIMEOUT; }
void sys_sem_free(sys_sem_t *sem)   { kfree(*sem); }

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    uint32_t start = sys_now();
    if (timeout == 0) {
        semaphore_wait(*sem);
        return sys_now() - start;
    }
    while (!semaphore_try_wait(*sem)) {
        if ((u32_t)(sys_now() - start) >= timeout)
            return SYS_ARCH_TIMEOUT;
        arch_yield();
    }
    return sys_now() - start;
}

struct sys_mbox {
    void **buf;
    int    size;
    int    head;
    int    tail;
    int    count;
    spinlock_t lock;
};

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if (!mbox || size <= 0) return ERR_ARG;
    *mbox = NULL;
    struct sys_mbox *m = kmalloc(sizeof(*m));
    if (!m) return ERR_MEM;
    m->buf = kmalloc((size_t)size * sizeof(void *));
    if (!m->buf) { kfree(m); return ERR_MEM; }
    m->size = size;
    m->head = 0;
    m->tail = 0;
    m->count = 0;
    debugln("[LWIP] Mailbox created: %p, size=%d\n", (void *)m, size);
    spinlock_init(&m->lock);
    *mbox = m;
    return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    struct sys_mbox *m = (struct sys_mbox *)*mbox;
    spinlock_acquire(&m->lock);
    m->buf[m->head] = msg;
    m->head = (m->head + 1) % m->size;
    m->count++;
    spinlock_release(&m->lock);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    if (!mbox || !*mbox) return ERR_ARG;
    if (!mbox || !*mbox) {
       debugln("[LWIP] ERROR: trypost received NULL mailbox: %p\n", (void *)mbox);
       return ERR_ARG;
    }
    struct sys_mbox *m = *mbox;
    spinlock_acquire(&m->lock);
    if (m->count >= m->size) { spinlock_release(&m->lock); return ERR_MEM; }
    m->buf[m->head] = msg;
    m->head = (m->head + 1) % m->size;
    m->count++;
    spinlock_release(&m->lock);
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg) {
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    struct sys_mbox *m = (struct sys_mbox *)*mbox;
    uint32_t start = sys_now();
    for (;;) {
        spinlock_acquire(&m->lock);
        if (m->count > 0) {
            *msg = m->buf[m->tail];
            m->tail = (m->tail + 1) % m->size;
            m->count--;
            spinlock_release(&m->lock);
            return 0;
        }
        spinlock_release(&m->lock);

        if (timeout != 0 && (u32_t)(sys_now() - start) >= timeout)
            return SYS_ARCH_TIMEOUT;
    }
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
    struct sys_mbox *m = (struct sys_mbox *)*mbox;
    spinlock_acquire(&m->lock);
    if (m->count == 0) { spinlock_release(&m->lock); return SYS_ARCH_TIMEOUT; }
    *msg = m->buf[m->tail];
    m->tail = (m->tail + 1) % m->size;
    m->count--;
    spinlock_release(&m->lock);
    return 0;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    struct sys_mbox *m = (struct sys_mbox *)*mbox;
    kfree(m->buf);
    kfree(m);
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                            void *arg, int stacksize, int prio) {
    process_t *proc = kthread_create((void (*)(void*))thread, arg, name);
    return (sys_thread_t)proc;
}

u32_t lwip_rand(void) {
    uint64_t val;
    return (u32_t)rand();
}

void sys_init(void) { }
