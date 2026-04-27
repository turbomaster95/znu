#ifndef _STDIO_H
#define _STDIO_H 1

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define ROUND_DOWN(v, n) ((v) - ((v) % (n)))
#define ROUND_UP(v, n) ROUND_DOWN((v) + (n) - 1, n)

int printf(const char* format, ...);
char* fgets(char* str, int n, int fd);

#endif
