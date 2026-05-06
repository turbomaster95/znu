#include <stdint.h>
#include <stdlib.h>

uintptr_t __stack_chk_guard = 0x5fdeadbeef;

__attribute__((noreturn))
void __stack_chk_fail(void) {
    panic("Stack smashing detected!");
}
