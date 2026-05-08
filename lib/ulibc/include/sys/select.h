#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>

#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

typedef long fd_mask;
#define _NFDBITS (sizeof(fd_mask) * 8)

typedef struct {
    fd_mask fds_bits[FD_SETSIZE / _NFDBITS];
} fd_set;

#define FD_ZERO(set) \
    do { \
        int __i; \
        for (__i = 0; __i < (FD_SETSIZE / _NFDBITS); __i++) \
            (set)->fds_bits[__i] = 0; \
    } while (0)

#define FD_SET(fd, set) \
    ((set)->fds_bits[(fd) / _NFDBITS] |= (1L << ((fd) % _NFDBITS)))

#define FD_CLR(fd, set) \
    ((set)->fds_bits[(fd) / _NFDBITS] &= ~(1L << ((fd) % _NFDBITS)))

#define FD_ISSET(fd, set) \
    (((set)->fds_bits[(fd) / _NFDBITS] & (1L << ((fd) % _NFDBITS))) != 0)

// Function prototypes
int select(int nfds, fd_set *readfds, fd_set *writefds, 
           fd_set *exceptfds, struct timeval *timeout);

#endif
