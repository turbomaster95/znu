#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define ROUND_DOWN(v, n) ((v) - ((v) % (n)))
#define ROUND_UP(v, n) ROUND_DOWN((v) + (n) - 1, n)
#define ANSI_BOLD      "\033[1m"
#define ANSI_RESET     "\033[0m"

int printf(const char* __restrict, ...);
int putchar(int);
int puts(const char*);
int vprintf(const char* restrict format, va_list parameters);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif
