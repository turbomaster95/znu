#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>

__attribute__((__noreturn__))
void panic(const char* reason);
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

// Output a 16-bit Word to an I/O port
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Input a 16-bit Word from an I/O port
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Output a 32-bit Double Word to an I/O port
static inline void outd(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

// Input a 32-bit Double Word from an I/O port
static inline uint32_t ind(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void debug_putchar(char c);
void debugerr(const char* format, ...);
void debugwarn(const char* format, ...);
void debugln(const char* format, ...);
void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi);
uint64_t rdmsr(uint32_t msr);
int vdebugprintf(const char* format, va_list args);

#endif

#endif
