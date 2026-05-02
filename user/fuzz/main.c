#include <stdint.h>
#include <stdio.h>

// Simple Xorshift for random numbers without needing a libc rand()
static uint64_t state = 0x1337BEEF;
uint64_t xorshift64() {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

int main() {
    printf("[fuzz] Chaos Fuzzer...\n");
    printf("[fuzz] Warning: This will likely panic the kernel.\n");

    while (1) {
        uint64_t nr   = xorshift64() % 256; // Fuzz first 256 syscalls
	if (nr == 48 || nr == 60 || nr == 231) continue;
        uint64_t arg1 = xorshift64();
        uint64_t arg2 = xorshift64();
        uint64_t arg3 = xorshift64();
        uint64_t arg4 = xorshift64();

        // Direct syscall invocation
        asm volatile (
            "movq %0, %%rax\n"
            "movq %1, %%rdi\n"
            "movq %2, %%rsi\n"
            "movq %3, %%rdx\n"
            "movq %4, %%r10\n"
            "syscall"
            :
            : "r"(nr), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
            : "rax", "rdi", "rsi", "rdx", "r10", "rcx", "r11", "memory"
        );
    }
    return 0;
}
