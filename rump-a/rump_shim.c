#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <rump/rumpuser.h>

/* --- Memory --- */
void *rumpuser_malloc(size_t size, int flags) { return malloc(size); }
void rumpuser_free(void *ptr, size_t size) { free(ptr); }
void *rumpuser_anonmmap(size_t size, int flags) { return malloc(size); }
void rumpuser_unmap(void *ptr, size_t size) { free(ptr); }

/* --- Threading & Synchronization --- */
int rumpuser_mutex_init(void **mtx, int flags) {
    *mtx = malloc(sizeof(pthread_mutex_t));
    return pthread_mutex_init((pthread_mutex_t *)*mtx, NULL);
}
void rumpuser_mutex_enter(void *mtx) { pthread_mutex_lock((pthread_mutex_t *)mtx); }
void rumpuser_mutex_exit(void *mtx) { pthread_mutex_unlock((pthread_mutex_t *)mtx); }

/* --- Debugging & Utilities --- */
void rumpuser_putchar(int c) { putchar(c); }
void rumpuser_dprintf(int level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* --- Required Initialization --- */
int rumpuser_getparam(const char *name, void *buf, size_t len) { return -1; }
void rumpuser_exit(int status) { exit(status); }

// Note: You will need to implement others (like cv_init, rw_init) 
// as the linker complains. Follow the pattern above using pthreads!
