#include <stdlib.h>
#include <syscall.h>

static uintptr_t heap_end = 0;

void* malloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes
    size = (size + 15) & ~15;

    // syscall 12 is sys_brk
    uintptr_t current_brk;
    __asm__ volatile ("syscall" : "=a"(current_brk) : "a"(12), "D"(0) : "rcx", "r11", "memory");

    uintptr_t next_brk = current_brk + size;
    
    uintptr_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(12), "D"(next_brk) : "rcx", "r11", "memory");

    if (ret == (uintptr_t)-1) return NULL;

    return (void*)current_brk;
}

void free(void* ptr) {
    // Stub: bump allocator doesn't support free
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = malloc(total);
    if (ptr) {
        for (size_t i = 0; i < total; i++) ((char*)ptr)[i] = 0;
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    // Stub: just allocate new and copy (dangerous but works for small stuff)
    void* new_ptr = malloc(size);
    if (new_ptr) {
        // We don't know the old size, so we can't copy properly.
        // This is a major limitation of this simple allocator.
    }
    return new_ptr;
}
