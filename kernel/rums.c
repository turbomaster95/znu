#include <stdarg.h>

// Forward declare your Znu logging functions so the stubs can see them
extern void debugln(const char *fmt, ...);
extern void putchar(char c); // Or whatever your native single-character output function is

/**
 * rumpuser_putchar:
 * Called by Rump for direct, raw character output (low-level console writes).
 */
void rumpuser_putchar(int c) {
    putchar((char)c);
}

/**
 * rumpuser_dprintf:
 * The primary logging engine for the Rump hyperkernel. It passes a format string
 * and a variable argument list, exactly like a standard printf.
 */
void rumpuser_dprintf(const char *format, ...) {
    // We bridge Rump's variadic logging straight into Znu's debugln engine.
    // If your debugln doesn't accept standard format strings directly,
    // you can process it via a local vsnprintf buffer first.
    va_list args;
    va_start(args, format);
    
    // Assuming your debugln/printf handling supports internal variadic routing:
    // If it takes va_list, use your vdebugln version here. Otherwise, a simple wrapper:
    debugln(format, args); 
    
    va_end(args);
}

/**
 * rumpuser_panic:
 * Called when Rump hits an unrecoverable internal kernel assertion failure.
 */
void rumpuser_panic(const char *filename, int line, const char *funname, const char *fmt, ...) {
    debugln("\n!!! RUMP KERNEL PANIC !!!\n");
    debugln("File: %s (Line: %d)\n", filename, line);
    debugln("Function: %s\n", funname);
    
    // Print out the explicit reason for the panic
    va_list args;
    va_start(args, fmt);
    debugln(fmt, args);
    va_end(args);

    // Halt the current CPU core immediately to prevent memory corruption
    debugln("\nSystem halted via Rump Panic.");
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}
