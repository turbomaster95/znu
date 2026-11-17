#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))

_Noreturn void panic(const char* reason);
_Noreturn void abort(void);

#include <arch/io.h>

void debug_putchar(char c);
void debugerr(const char* format, ...);
void debugwarn(const char* format, ...);
void debugln(const char* format, ...);
void vdebugln(const char* level, const char* tag, const char* format, va_list args);
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
