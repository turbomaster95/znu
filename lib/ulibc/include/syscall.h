#include <stddef.h>

static inline size_t sys_write(int fd, const void* buf, size_t count) {
    size_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline size_t sys_read(int fd, void* buf, size_t count) {
    size_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(0), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline size_t sys_reboot() {
    size_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(169)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline size_t sys_shutdown() {
    size_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(48)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int sys_open(const char* path, int flags) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(2), "D"(path), "S"(flags) : "rcx", "r11", "memory");
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(3), "D"(fd) : "rcx", "r11", "memory");
    return ret;
}

typedef struct {
    char name[128];
    unsigned int type;
    unsigned int size;
} znu_dirent_t;

static inline int sys_getdents(int fd, void* buf, size_t count) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(217), "D"(fd), "S"(buf), "d"(count) : "rcx", "r11", "memory");
    return ret;
}

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[20-2*sizeof(long)-sizeof(int)];
};

static inline int sys_sysinfo(struct sysinfo* info) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(99), "D"(info) : "rcx", "r11", "memory");
    return ret;
}

static inline void sys_exit(int status) {
    __asm__ volatile ("syscall" : : "a"(60), "D"(status) : "rcx", "r11", "memory");
}

static inline int sys_spawn(const char* path) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(59), "D"(path) : "rcx", "r11", "memory");
    return ret;
}

static inline int sys_wait(int pid) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(61), "D"(pid) : "rcx", "r11", "memory");
    return ret;
}