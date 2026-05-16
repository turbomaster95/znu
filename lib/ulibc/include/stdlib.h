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
    
    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return res * sign;
}

static unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    unsigned long long res = 0;
    while (*nptr == ' ') nptr++;
    if (*nptr == '+') nptr++;
    
    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return res;
}

static long long strtoimax(const char *nptr, char **endptr, int base) {
    return strtoll(nptr, endptr, base);
}

static unsigned long long strtoumax(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}
static inline unsigned long strtoul(const char *nptr, char **endptr, int base) {
    // Forward directly to your existing 64-bit uintmax parser
    return (unsigned long)strtoumax(nptr, endptr, base);
}
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
#define alloca __builtin_alloca
void abort(void);
void exit(int status);
void _exit(int status);
int atoi(const char *nptr);
char* getenv(const char* name);
int system(const char* command);
int abs(int j);
long labs(long j);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
