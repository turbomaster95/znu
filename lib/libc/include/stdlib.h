#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>

__attribute__((__noreturn__))
void panic(void);
void abort(void);

#if defined(__is_libk)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

void debug_putchar(char c);
void debugerr(const char* format, ...);
void debugwarn(const char* format, ...);
void debugln(const char* format, ...);

int vdebugprintf(const char* format, va_list args);

#endif

#endif
