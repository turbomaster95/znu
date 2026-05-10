#include <stdio.h>
#include <stdlib.h>
#include <kernel/display.h>

#define PANIC_BG_COLOR 0xff0000

/**
 * panic: Unrecoverable kernel error.
 * This is only compiled into the kernel (libk).
 */
__attribute__((__noreturn__))
void panic(const char* reason) {
#if defined(__is_libk)
    draw_rect(0, 0, 1920, 1080, PANIC_BG_COLOR);

    printf("\n!!!! [KERNEL PANIC] !!!!\n");
    if (reason) {
        printf("REASON: %s\n", reason);
    }
    printf("\nSystem Halted.\n");

    debugerr("KERNEL PANIC: %s", reason ? reason : "No reason provided");

    asm volatile("cli");
    for (;;) {
        asm volatile("hlt");
    }
#else
    printf("User-space attempted to call kernel panic().\n");
    abort();
#endif
    __builtin_unreachable();
}

