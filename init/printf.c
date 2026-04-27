#include "stdio.h"
#include <stdarg.h>


void putchar(char c) {
    register long rax __asm__("rax") = 1;
    register long rdi __asm__("rdi") = c;
    __asm__ volatile (
        "syscall"
        : "+r"(rax)
        : "r"(rdi)
        : "rcx", "r11", "rdx", "rsi", "memory"
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
