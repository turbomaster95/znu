#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

// ANSI Color Codes
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_RESET   "\x1b[0m"

void debug_putchar(char c) {
    outb(0xe9, c);
}

void debug_write(const char* data) {
    while (*data) {
        debug_putchar(*data++);
    }
}

// Internal helper to handle the colored logging logic
static void _debug_vlog_colored(const char* color, const char* prefix, const char* format, va_list args) {
//    debug_write(color);
//    debug_write("[");
//    debug_write(prefix);
//    debug_write("] ");
    debug_write(ANSI_RESET);

    // FIX: Use vdebugprintf so text goes to Port 0xe9, not the screen
    vdebugprintf(format, args); 

    debug_write("\n");
}

void debugwarn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _debug_vlog_colored(ANSI_YELLOW, "KERNEL WARN", format, args);
    va_end(args);
}

void debugln(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _debug_vlog_colored(ANSI_GREEN, "KERNEL LOG", format, args);
    va_end(args);
}

// Update debugerr and others to use the same va_list pattern
void debugerr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _debug_vlog_colored(ANSI_RED, "KERNEL ERR", format, args);
    va_end(args);
}

void debuginfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _debug_vlog_colored(ANSI_BLUE, "KERNEL INFO", format, args);
    va_end(args);
}

