#include <stdio.h>
#include <stdlib.h>
#include <kernel/tty.h>


#define BG color_hex(0xff0000)

__attribute__((__noreturn__))
void panic(void) {
#if defined(__is_libk)
        // TODO: Add proper kernel panic.
        draw_rect(0, 0, 1920, 1080, BG);
        printf("!!!![KERNEL PANIC]!!!!\n");
	debugerr("KERNEL PANIC!!");
        asm volatile("hlt");
#else
        printf("Kernel Function tried to be called from non libk\n");
#endif
        while (1) { }
        __builtin_unreachable();
}
