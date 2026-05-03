#include "stdio.h"
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

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

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    size_t count = 0;
    const char* p = format;

    while (*p != '\0') {
        if (*p == '%' && *(p + 1) != '\0') {
            p++;
            
            int zero_pad = 0;
            int width = 0;
            if (*p == '0') {
                zero_pad = 1;
                p++;
            }
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }

            int is_long = 0;
            int is_long_long = 0;
            int is_size_t = 0;
            if (*p == 'l') {
                is_long = 1;
                p++;
                if (*p == 'l') {
                    is_long_long = 1;
                    p++;
                }
            } else if (*p == 'z') {
                is_size_t = 1;
                p++;
            }

            if (*p == 'c') {
                char c = (char)va_arg(ap, int);
                if (str && count < size - 1) str[count] = c;
                count++;
            } 
            else if (*p == 's') {
                char* s = va_arg(ap, char*);
                if (!s) s = "(null)";
                while (*s) {
                    if (str && count < size - 1) str[count] = *s;
                    s++;
                    count++;
                }
            } 
            else if (*p == 'd' || *p == 'i' || *p == 'u' || *p == 'x' || *p == 'X' || *p == 'p') {
                uint64_t num;
                int negative = 0;
                int base = 10;
                int uppercase = 0;

                if (*p == 'd' || *p == 'i') {
                    int64_t s_num;
                    if (is_long_long) s_num = va_arg(ap, int64_t);
                    else if (is_long) s_num = va_arg(ap, long);
                    else if (is_size_t) s_num = va_arg(ap, size_t);
                    else s_num = va_arg(ap, int);
                    
                    if (s_num < 0) {
                        negative = 1;
                        num = (uint64_t)(-s_num);
                    } else {
                        num = (uint64_t)s_num;
                    }
                } else {
                    if (*p == 'x' || *p == 'X' || *p == 'p') base = 16;
                    if (*p == 'X') uppercase = 1;
                    if (*p == 'p') {
                        num = (uintptr_t)va_arg(ap, void*);
                        if (str && count < size - 1) str[count] = '0'; count++;
                        if (str && count < size - 1) str[count] = 'x'; count++;
                    } 
                    else if (is_long_long) num = va_arg(ap, uint64_t);
                    else if (is_long) num = va_arg(ap, unsigned long);
                    else if (is_size_t) num = va_arg(ap, size_t);
                    else num = va_arg(ap, unsigned int);
                }

                char buf[64];
                int i = 0;
                if (num == 0) buf[i++] = '0';
                else {
                    while (num > 0) {
                        int rem = num % base;
                        if (rem < 10) buf[i++] = rem + '0';
                        else buf[i++] = rem - 10 + (uppercase ? 'A' : 'a');
                        num /= base;
                    }
                }

                if (negative) {
                    if (str && count < size - 1) str[count] = '-';
                    count++;
                }

                while (i < width) {
                    if (str && count < size - 1) str[count] = zero_pad ? '0' : ' ';
                    count++;
                    width--;
                }

                while (i > 0) {
                    if (str && count < size - 1) str[count] = buf[--i];
                    count++;
                }
            } 
            else if (*p == '%') {
                if (str && count < size - 1) str[count] = '%';
                count++;
            }
        } else {
            if (str && count < size - 1) str[count] = *p;
            count++;
        }
        p++;
    }

    if (str && size > 0) {
        if (count < size) str[count] = '\0';
        else str[size - 1] = '\0';
    }

    return (int)count;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, (size_t)-1, format, ap);
}

int printf(const char* format, ...) {
    char buf[1024]; // hope it's enough
    va_list args;
    va_start(args, format);
    int count = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    for (int i = 0; i < count && i < (int)sizeof(buf) - 1; i++) {
        putchar(buf[i]);
    }
    return count;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = vsnprintf(str, size, format, args);
    va_end(args);
    return count;
}

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = vsprintf(str, format, args);
    va_end(args);
    return count;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[1024];
    int count = vsnprintf(buf, sizeof(buf), format, ap);
    register long rax __asm__("rax") = 1; // sys_write
    register long rdi __asm__("rdi") = (long)stream;
    register long rsi __asm__("rsi") = (long)buf;
    register long rdx __asm__("rdx") = (long)count;
    __asm__ volatile (
        "syscall"
        : "+r"(rax)
        : "r"(rdi), "r"(rsi), "r"(rdx)
        : "rcx", "r11", "memory"
    );
    return count;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = vfprintf(stream, format, args);
    va_end(args);
    return count;
}

int puts(const char* string) {
    return printf("%s\n", string);
}

int fputs(const char* s, FILE* stream) {
    while (*s) {
        char c = *s++;
        register long rax __asm__("rax") = 1; // sys_write
        register long rdi __asm__("rdi") = (long)stream;
        register long rsi __asm__("rsi") = (long)&c;
        register long rdx __asm__("rdx") = 1;
        __asm__ volatile (
            "syscall"
            : "+r"(rax)
            : "r"(rdi), "r"(rsi), "r"(rdx)
            : "rcx", "r11", "memory"
        );
    }
    return 0;
}
