#ifndef _KERNEL_KAPI_H
#define _KERNEL_KAPI_H

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

extern void debugln(const char *fmt, ...);
extern void kprintf(const char *fmt, ...);
extern void panic(const char *fmt, ...);

extern void *kmalloc(size_t size);
extern void kfree(void *ptr);
extern void *krealloc(void *ptr, size_t size);

extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern size_t strlen(const char *s);
extern char *strcpy(char *dest, const char *src);
extern char *strncpy(char *dest, const char *src, size_t n);

extern void spinlock_acquire(uint32_t *lock);
extern void spinlock_release(uint32_t *lock);

#endif
