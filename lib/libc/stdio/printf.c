/* lib/libc/stdio/printf.c */
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// Ensure printf.c knows about debug_putchar
extern void debug_putchar(char c);

static void itoa(uint64_t n, char* str, int base) {
    char *ptr = str, *low = str;
    const char* digits = "0123456789abcdef";
    do { *ptr++ = digits[n % base]; n /= base; } while (n);
    *ptr-- = '\0';
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
}

static int base_vprintf(void (*putc)(char), const char* restrict format, va_list parameters) {
    int written = 0;
    while (*format != '\0') {
        if (format[0] != '%') {
            putc(*format++);
            written++;
            continue;
        }

        if (format[1] == '%') {
            putc('%');
            format += 2;
            written++;
            continue;
        }

        format++; // Move past '%'

        // --- Length Modifier Detection ---
        int length_mod = 0; // 0 = default, 1 = long (l), 2 = long long (ll)
        if (*format == 'l') {
            length_mod = 1;
            format++;
            if (*format == 'l') {
                length_mod = 2;
                format++;
            }
        }

        if (*format == 'c') {
            format++;
            putc((char)va_arg(parameters, int));
            written++;
        } else if (*format == 's') {
            format++;
            const char* str = va_arg(parameters, const char*);
            if (!str) str = "(null)";
            while (*str) { putc(*str++); written++; }
        } else if (*format == 'd' || *format == 'i' || *format == 'x' || *format == 'p' || *format == 'u') {
            char type = *format++;
            uint64_t n;
            char buf[64];

            if (type == 'd' || type == 'i') {
                int64_t val;
                // If it's %ld or %lld, pull 64 bits, otherwise pull 32 bits
                if (length_mod >= 1) val = va_arg(parameters, int64_t);
                else val = va_arg(parameters, int32_t);

                if (val < 0) { putc('-'); written++; val = -val; }
                n = (uint64_t)val;
            } else if (type == 'p') {
                n = (uintptr_t)va_arg(parameters, void*);
                putc('0'); putc('x'); written += 2;
            } else {
                // Handle %lu or %llu
                if (length_mod >= 1) n = va_arg(parameters, uint64_t);
                else n = va_arg(parameters, uint32_t);
            }

            itoa(n, buf, (type == 'x' || type == 'p') ? 16 : 10);
            for (int i = 0; buf[i]; i++) { putc(buf[i]); written++; }
        }
    }
    return written;
}

int printf(const char* restrict format, ...) {
    va_list args;
    va_start(args, format);
    int result = base_vprintf((void(*)(char))putchar, format, args);
    va_end(args);
    return result;
}

int vprintf(const char* restrict format, va_list parameters) {
    return base_vprintf((void(*)(char))putchar, format, parameters);
}

int vdebugprintf(const char* restrict format, va_list parameters) {
    return base_vprintf(debug_putchar, format, parameters);
}

/* Helper for vsnprintf */
struct snprintf_ctx {
    char *buf;
    size_t size;
    size_t written;
};

static struct snprintf_ctx _sn_ctx;

static void sn_putc(char c) {
    if (_sn_ctx.written < _sn_ctx.size - 1) {
        _sn_ctx.buf[_sn_ctx.written] = c;
    }
    _sn_ctx.written++;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    if (size == 0) return 0;

    _sn_ctx.buf = str;
    _sn_ctx.size = size;
    _sn_ctx.written = 0;

    base_vprintf(sn_putc, format, ap);

    // Null terminate
    if (_sn_ctx.written < _sn_ctx.size) {
        str[_sn_ctx.written] = '\0';
    } else {
        str[_sn_ctx.size - 1] = '\0';
    }

    return _sn_ctx.written;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}
