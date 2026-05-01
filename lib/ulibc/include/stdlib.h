#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <stdint.h>

static long long strtoll(const char *nptr, char **endptr, int base) {
    long long res = 0;
    int sign = 1;
    while (*nptr == ' ') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;
    
    // Simplistic base 10/16/8 logic
    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return res * sign;
}

void* malloc(size_t size);
void free(void* ptr);
void abort(void);
void exit(int status);
void _exit(int status);
char* getenv(const char* name);
int system(const char* command);
int abs(int j);
long labs(long j);
int atoi(const char *nptr);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
