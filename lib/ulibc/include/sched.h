#ifndef SCHED_H
#define SCHED_H

#include <sys/types.h>

#define CPU_SETSIZE 1024
typedef unsigned long __cpu_mask;
#define __NCPUBITS (8 * sizeof(__cpu_mask))

typedef struct {
    __cpu_mask __bits[CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

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

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
int sched_yield(void);

#endif // SCHED_H
