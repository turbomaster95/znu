#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))

__attribute__((__noreturn__))
void panic(const char* reason);
void abort(void);

#if defined(__is_libk)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insb(uint16_t port, void *addr, uint32_t cnt) {
    __asm__ volatile ("rep insb" 
                      : "+D"(addr), "+c"(cnt) 
                      : "d"(port) 
                      : "memory");
}

static inline void outsb(uint16_t port, const void *addr, uint32_t cnt) {
    __asm__ volatile ("rep outsb" 
                      : "+S"(addr), "+c"(cnt) 
                      : "d"(port));
}

static inline void insw(uint16_t port, void *addr, uint32_t cnt) {
    __asm__ volatile ("rep insw" 
                      : "+D"(addr), "+c"(cnt) 
                      : "d"(port) 
                      : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint32_t cnt) {
    __asm__ volatile ("rep outsw" 
                      : "+S"(addr), "+c"(cnt) 
                      : "d"(port));
}

static inline void insl(uint16_t port, void *addr, uint32_t cnt) {
    __asm__ volatile ("rep insl" 
                      : "+D"(addr), "+c"(cnt) 
                      : "d"(port) 
                      : "memory");
}

static inline void outsl(uint16_t port, const void *addr, uint32_t cnt) {
    __asm__ volatile ("rep outsl" 
                      : "+S"(addr), "+c"(cnt) 
                      : "d"(port));
}

void debug_putchar(char c);
void debugerr(const char* format, ...);
void debugwarn(const char* format, ...);
void debugln(const char* format, ...);
void debug_write(const char* data);
void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi);
uint64_t rdmsr(uint32_t msr);
void write_cr4(uint64_t cr4);
uint64_t read_cr4(void);
void disable_smap(void);
void enable_smap(void);
int vdebugprintf(const char* format, va_list args);
int rand(void);
void srand(unsigned int seed);
int rdrand64(uint64_t* val);
void seed_from_hardware(void);
void entropy_garden(void);

#endif

#endif
