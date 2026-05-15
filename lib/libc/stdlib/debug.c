#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <kernel/display.h>
#include <sync.h>
#include <lapic.h>

bool vmm_ready = false;
extern spinlock_t terminal_print_lock;
extern void smp_enqueue_log(const char* str);

static void raw_putchar(char c) {
    outb(0xe9, c);
    if (vmm_ready && get_cpu_id() == 0) {
        if (c == '\n') terminal_putchar('\r');
        terminal_putchar(c);
    }
}

static void raw_write_string(const char* str) {
    while (*str) {
        raw_putchar(*str++);
    }
}

void debug_putchar(char c) {
    uint64_t flags = spinlock_irq_save(&terminal_print_lock);
    raw_putchar(c);
    spinlock_irq_restore(&terminal_print_lock, flags);
}

void debug_putcharn(char c) {
    outb(0xe9, c);
}

void debug_write(const char* data) {
    uint64_t flags = spinlock_irq_save(&terminal_print_lock);
    raw_write_string(data);
    spinlock_irq_restore(&terminal_print_lock, flags);
}

static void do_safe_log(const char* prefix, const char* format, va_list args) {
    char log_buffer[1024]; 
    int written = 0;

    if (format[0] == '[') {
        const char* end_bracket = strchr(format, ']');
        if (end_bracket) {
            written += snprintf(log_buffer + written, sizeof(log_buffer) - written, "%s[", ANSI_BOLD);
            
            const char* p = format + 1;
            while (p < end_bracket && written < (int)sizeof(log_buffer) - 10) {
                log_buffer[written++] = *p++;
            }
            
            written += snprintf(log_buffer + written, sizeof(log_buffer) - written, "]%s", ANSI_RESET);
            
            written += vsnprintf(log_buffer + written, sizeof(log_buffer) - written, end_bracket + 1, args);
        } else {
            written += vsnprintf(log_buffer + written, sizeof(log_buffer) - written, format, args);
        }
    } else {
        if (prefix) {
            written += snprintf(log_buffer + written, sizeof(log_buffer) - written, "%s", prefix);
        }
        written += vsnprintf(log_buffer + written, sizeof(log_buffer) - written, format, args);
    }

    if (written < (int)sizeof(log_buffer) - 2) {
        log_buffer[written++] = '\n';
        log_buffer[written] = '\0';
    }

    if (get_cpu_id() == 0) {
        uint64_t flags = spinlock_irq_save(&terminal_print_lock);
        raw_write_string(log_buffer);
        spinlock_irq_restore(&terminal_print_lock, flags);
    } else {
        smp_enqueue_log(log_buffer);
    }
}

void debugln(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_safe_log(NULL, format, args);
    va_end(args);
}

void debugwarn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_safe_log("\033[1;33m[WARN] \033[0m", format, args);
    va_end(args);
}

void debugerr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_safe_log("\033[1;31m[ERROR] \033[0m", format, args);
    va_end(args);
}

void debuginfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_safe_log(NULL, format, args);
    va_end(args);
}
