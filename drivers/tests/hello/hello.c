#include <stdint.h>

extern void debugln(const char *fmt, ...);
extern void kprintf(const char *fmt, ...);

int module_init(void)
{
    debugln("Hello from kernel module!");
    return 0;
}

void module_exit(void)
{
    debugln("Goodbye from kernel module!");
}
