#ifndef _USERLIBC_MALLOC_H
#define _USERLIBC_MALLOC_H

#include <stddef.h>

// mallopt parameters used by BusyBox / GNU allocations
#define M_TRIM_THRESHOLD    -1
#define M_TOP_PAD           -2
#define M_MMAP_THRESHOLD    -3
#define M_MMAP_MAX          -4

// POSIX/GNU function prototypes
void *malloc(size_t size);
void  free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// Memory allocator tuning hook
int mallopt(int param, int value);

#endif // _USERLIBC_MALLOC_H
