#ifndef __LWIP_ARCH_SYS_ARCH_H__
#define __LWIP_ARCH_SYS_ARCH_H__

#include "lwip/opt.h"
#include "lwip/sys.h"
#include <sync.h>
#include <stdint.h>
#include <proc.h>

typedef void (*lwip_thread_fn)(void *arg);

typedef uintptr_t sys_prot_t;
typedef process_t *sys_thread_t;
typedef mutex_t     *sys_mutex_t;
typedef semaphore_t *sys_sem_t;
typedef struct sys_mbox *sys_mbox_t;
typedef uint32_t    sys_time_t;

err_t sys_mutex_new(sys_mutex_t *mutex);
void  sys_mutex_lock(sys_mutex_t *mutex);
void  sys_mutex_unlock(sys_mutex_t *mutex);
void  sys_mutex_free(sys_mutex_t *mutex);

err_t sys_sem_new(sys_sem_t *sem, u8_t count);
void  sys_sem_signal(sys_sem_t *sem);
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout);
u32_t sys_sem_trywait(sys_sem_t *sem);
void  sys_sem_free(sys_sem_t *sem);

err_t sys_mbox_new(sys_mbox_t *mbox, int size);
void  sys_mbox_post(sys_mbox_t *mbox, void *msg);
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg);
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg);
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout);
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg);
void  sys_mbox_free(sys_mbox_t *mbox);

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                            void *arg, int stacksize, int prio);

u32_t sys_jiffies(void);
u32_t sys_now(void);

sys_prot_t sys_arch_protect(void);
void       sys_arch_unprotect(sys_prot_t pval);

void sys_init(void);

#endif
