#include <stdio.h>
#include <stdlib.h>
#include <kernel/display.h>
#include <lapic.h>
#include <sync.h>

#define PANIC_BG_COLOR 0xff0000

__attribute__((__noreturn__))
void panic(const char* reason) {
    draw_rect(0, 0, 1920, 1080, PANIC_BG_COLOR);

    lapic_send_panic_ipi();

    extern spinlock_t terminal_print_lock;
    terminal_print_lock.locked = 0;

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
}

