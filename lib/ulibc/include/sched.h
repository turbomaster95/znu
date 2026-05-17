#ifndef _USERLIBC_SCHED_H
#define _USERLIBC_SCHED_H

#include <sys/types.h>

// CPU Set bitmask structure size configuration
#define CPU_SETSIZE 1024
typedef unsigned long __cpu_mask;
#define __NCPUBITS (8 * sizeof(__cpu_mask))

typedef struct {
    __cpu_mask __bits[CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

// Standard POSIX CPU bitmask manipulation macros
#define CPU_ZERO(cpusetp) \
    do { \
        size_t __i; \
        for (__i = 0; __i < sizeof(cpu_set_t)/sizeof(__cpu_mask); __i++) \
            (cpusetp)->__bits[__i] = 0; \
    } while (0)

#define CPU_SET(cpu, cpusetp) \
    ((cpusetp)->__bits[(cpu) / __NCPUBITS] |= (1UL << ((cpu) % __NCPUBITS)))

#define CPU_CLR(cpu, cpusetp) \
    ((cpusetp)->__bits[(cpu) / __NCPUBITS] &= ~(1UL << ((cpu) % __NCPUBITS)))

#define CPU_ISSET(cpu, cpusetp) \
    (((cpusetp)->__bits[(cpu) / __NCPUBITS] & (1UL << ((cpu) % __NCPUBITS))) != 0)

// Function prototypes BusyBox expects for process affinity management
int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
int sched_yield(void);

#endif // _USERLIBC_SCHED_H
