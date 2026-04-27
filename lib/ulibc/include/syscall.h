#include <stddef.h>

static inline size_t sys_read(int fd, void* buf, size_t count) {
    size_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(3), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}
