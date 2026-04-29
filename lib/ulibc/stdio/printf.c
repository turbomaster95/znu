#include "stdio.h"
#include <stdarg.h>

void putchar(char c) {
    register long rax __asm__("rax") = 1; // sys_write
    register long rdi __asm__("rdi") = 1; // stdout
    register long rsi __asm__("rsi") = (long)&c; // buffer
    register long rdx __asm__("rdx") = 1; // count
    __asm__ volatile (
        "syscall"
        : "+r"(rax)
        : "r"(rdi), "r"(rsi), "r"(rdx)
        : "rcx", "r11", "memory"
    );
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;

    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++; // Skip '%'
            if (*p == 'c') {
                // Get char from args and print it
                char c = (char)va_arg(args, int);
                putchar(c);
                count++;
            } else if (*p == 's') {
                // Get string pointer and print char by char
                char* s = va_arg(args, char*);
                while (*s) {
                    putchar(*s++);
                    count++;
                }
            } else if (*p == 'd') {
                int num = va_arg(args, int);
                if (num == 0) {
                    putchar('0');
                    count++;
                } else {
                    if (num < 0) {
                        putchar('-');
                        count++;
                        num = -num;
                    }
                    char buf[32];
                    int i = 0;
                    while (num > 0) {
                        buf[i++] = (num % 10) + '0';
                        num /= 10;
                    }
                    while (i > 0) {
                        putchar(buf[--i]);
                        count++;
                    }
                }
            } else if (*p == 'x') {
                unsigned int num = va_arg(args, unsigned int);
                if (num == 0) {
                    putchar('0');
                    count++;
                } else {
                    char buf[32];
                    int i = 0;
                    while (num > 0) {
                        int rem = num % 16;
                        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
                        num /= 16;
                    }
                    while (i > 0) {
                        putchar(buf[--i]);
                        count++;
                    }
                }
            } else if (*p == '%') {
                putchar('%');
                count++;
            }
        } else {
            // Normal character
            putchar(*p);
            count++;
        }
    }

    va_end(args);
    return count;
}

int puts(const char* string) {
        return printf("%s\n", string);
}
