#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <sys/types.h>

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
struct itimerval {
    struct timeval it_interval; // Next value interval
    struct timeval it_value;    // Current value countdown
};

// POSIX timestamp updates prototype
int utimes(const char *filename, const struct timeval times[2]);

#endif
