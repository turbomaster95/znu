#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <kernel/tty.h>

// ANSI Color Codes
bool vmm_ready = false;

void debug_putchar(char c) {
    outb(0xe9, c);
    if (vmm_ready) {
       terminal_putchar(c);
    }
}

void debug_write(const char* data) {
    while (*data) {
        debug_putchar(*data++);
    }
}

static void _debug_vlog_colored(const char* format, va_list args) {
    // Check if the string starts with a bracketed tag
    if (format[0] == '[') {
        const char* end_bracket = strchr(format, ']');
        
        if (end_bracket) {
            debug_write(ANSI_BOLD);

            debug_putchar('[');

            const char* p = format + 1;
            while (p < end_bracket) {
                debug_putchar(*p++);
            }

            debug_putchar(']');
            debug_write(ANSI_RESET);

            vdebugprintf(end_bracket + 1, args);
        } else {
            vdebugprintf(format, args);
        }
    } else {
        vdebugprintf(format, args);
    }
    debug_write("\n");
}

void debugln(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _debug_vlog_colored(format, args);
    va_end(args);
}

void debugwarn(const char* format, ...) {
    debug_write("\033[1;33m[WARN] \033[0m"); // Yellow tag for warnings
    va_list args;
    va_start(args, format);
    vdebugprintf(format, args);
    debug_write("\n");
    va_end(args);
}

void debugerr(const char* format, ...) {
    debug_write("\033[1;31m[ERROR] \033[0m"); // Red tag for errors
    va_list args;
    va_start(args, format);
    vdebugprintf(format, args);
    debug_write("\n");
    va_end(args);
}

void debuginfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _debug_vlog_colored(format, args);
    va_end(args);
}
