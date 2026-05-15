#ifndef _TIME_H
#define _TIME_H

#include <sys/types.h>

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
#ifndef _TIMESPEC_DECLARED
#define _TIMESPEC_DECLARED
struct timespec {
    time_t tv_sec;  /* Seconds */
    long   tv_nsec; /* Nanoseconds */
};
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

int clock_gettime(int clock_id, struct timespec *tp);
time_t time(time_t *tloc);
clock_t clock(void);
double difftime(time_t time1, time_t time2);
time_t mktime(struct tm *timeptr);
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr);

#endif
