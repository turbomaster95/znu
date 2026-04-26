#include <stdio.h>
#include <stdlib.h>

/**
 * abort: Abnormally terminate the current execution context.
 */
__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
    panic("abort() called in kernel-space");
#else
    printf("Process aborted.\n");

    // Assuming syscall #2 is your 'sys_exit' or 'sys_terminate'
    // This tells the kernel to reclaim memory instead of hanging the CPU.
    asm volatile (
        "mov $2, %%rax\n"
        "syscall"
        : : : "rax", "rcx", "r11"
    );

    // If the syscall fails to terminate the process, spin forever
    for (;;) { }
#endif
    __builtin_unreachable();
}

