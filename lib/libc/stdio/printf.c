#include <limits.h>
#include <kernel/display.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// Ensure printf.c knows about debug_putchar
extern void debug_putchar(char c);

static void itoa(uint64_t n, char* str, int base, bool uppercase) {
    char *ptr = str, *low = str;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    do {
        *ptr++ = digits[n % base];
        n /= base;
    } while (n);

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
        if (*format != '%') {
            putc(*format++);
            written++;
            continue;
        }

        format++; // Move past '%'

        // --- 1. Flags ---
        bool left_justify = false;
        bool force_sign = false;
        bool space_sign = false;
        bool hash_flag = false;
        bool zero_pad = false;

        while (true) {
            if (*format == '-') left_justify = true;
            else if (*format == '+') force_sign = true;
            else if (*format == ' ') space_sign = true;
            else if (*format == '#') hash_flag = true;
            else if (*format == '0') zero_pad = true;
            else break;
            format++;
        }

        // --- 2. Width ---
        int width = 0;
        if (*format == '*') {
            width = va_arg(parameters, int);
            format++;
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format++ - '0');
            }
        }

        // --- 3. Precision ---
        int precision = -1;
        if (*format == '.') {
            format++;
            if (*format == '*') {
                precision = va_arg(parameters, int);
                format++;
            } else {
                precision = 0;
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format++ - '0');
                }
            }
        }

        // --- 4. Length Modifiers ---
        int length_mod = 0; // 1=h, 2=hh, 3=l, 4=ll, 5=z, 6=j, 7=t
        if (*format == 'h') {
            length_mod = 1; format++;
            if (*format == 'h') { length_mod = 2; format++; }
        } else if (*format == 'l') {
            length_mod = 3; format++;
            if (*format == 'l') { length_mod = 4; format++; }
        } else if (*format == 'z') { length_mod = 5; format++; }
        else if (*format == 'j') { length_mod = 6; format++; }
        else if (*format == 't') { length_mod = 7; format++; }

        // --- 5. Conversion Specifiers ---
        char type = *format++;
        if (type == '%') {
            putc('%'); written++;
            continue;
        }

        if (type == 'c') {
            char c = (char)va_arg(parameters, int);
            putc(c); written++;
        } else if (type == 's') {
            const char* str = va_arg(parameters, const char*);
            if (!str) str = "(null)";
            int len = 0;
            while (str[len]) len++;
            if (precision >= 0 && precision < len) len = precision;

            if (!left_justify) while (width-- > len) { putc(' '); written++; }
            for (int i = 0; i < len; i++) { putc(str[i]); written++; }
            while (width-- > len) { putc(' '); written++; }
        } else if (type == 'd' || type == 'i' || type == 'u' || type == 'x' || type == 'X' || type == 'o' || type == 'p' || type == 'b') {
            uint64_t n;
            bool negative = false;
            int base = 10;

            if (type == 'x' || type == 'X' || type == 'p') base = 16;
            else if (type == 'o') base = 8;
            else if (type == 'b') base = 2;

            // Handle Signed
            if (type == 'd' || type == 'i') {
                int64_t val;
                if (length_mod == 4) val = va_arg(parameters, int64_t);
                else if (length_mod == 3) val = va_arg(parameters, long);
                else if (length_mod == 6) val = va_arg(parameters, intmax_t);
                else if (length_mod == 7) val = va_arg(parameters, ptrdiff_t);
                else val = va_arg(parameters, int);

                if (val < 0) { negative = true; n = (uint64_t)-val; }
                else n = (uint64_t)val;
            } else if (type == 'p') {
                n = (uintptr_t)va_arg(parameters, void*);
                hash_flag = true; // Pointers always get 0x
            } else {
                // Unsigned
                if (length_mod == 4) n = va_arg(parameters, uint64_t);
                else if (length_mod == 3) n = va_arg(parameters, unsigned long);
                else if (length_mod == 5) n = va_arg(parameters, size_t);
                else n = va_arg(parameters, unsigned int);
            }

            char buf[64];
            itoa(n, buf, base, (type == 'X'));

            int len = 0;
            while (buf[len]) len++;
            if (n == 0 && precision == 0) len = 0; // %d with .0 and value 0 prints nothing

            int actual_len = len;
            if (precision > len) actual_len = precision;
            if (negative || force_sign || space_sign) actual_len++;
            if (hash_flag && n != 0) {
                if (base == 16) actual_len += 2;
                else if (base == 8) actual_len += 1;
            }

            // --- Padding Start ---
            if (!left_justify && !zero_pad) while (width-- > actual_len) { putc(' '); written++; }
            
            if (negative) { putc('-'); written++; }
            else if (force_sign) { putc('+'); written++; }
            else if (space_sign) { putc(' '); written++; }

            if (hash_flag && (n != 0 || type == 'p')) {
                if (base == 16) { putc('0'); putc(type == 'X' ? 'X' : 'x'); written += 2; }
                else if (base == 8 && buf[0] != '0') { putc('0'); written++; }
            }

            if (!left_justify && zero_pad) while (width-- > actual_len) { putc('0'); written++; }
            while (precision-- > len) { putc('0'); written++; }

            for (int i = 0; i < len; i++) { putc(buf[i]); written++; }
            while (width-- > actual_len) { putc(' '); written++; }
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
