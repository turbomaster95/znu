#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>

extern void hcf(void);

#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_SFMASK       0xC0000084

// This structure will be what GS points to
typedef struct {
    uint64_t kernel_stack; // [gs:0x00]
    uint64_t user_stack;   // [gs:0x08]
} cpu_context_t;

static cpu_context_t main_cpu_context;

void gs_init(uintptr_t stack_top) {
    main_cpu_context.kernel_stack = stack_top;
    main_cpu_context.user_stack = 0;

    uintptr_t addr = (uintptr_t)&main_cpu_context;

    // We set MSR_KERNEL_GS_BASE because 'swapgs' swaps 
    // the current GS with this hidden value.
    wrmsr(0xC0000102, (uint32_t)addr, (uint32_t)(addr >> 32));
}

void syscall_init() {
    uint32_t lo, hi;

    // 1. Set up STAR: Kernel and User segments
    // Format: [User SS: 16 bits][Kernel CS: 16 bits][ignored: 32 bits]
    // Kernel CS = 0x08, User SS (base) = 0x18
    hi = (0x10 << 16) | 0x08;
    lo = 0;
    wrmsr(MSR_STAR, lo, hi);

    // 2. Set up LSTAR: The entry point for the 'syscall' instruction
    extern void syscall_entry(); // Assembly function
    uintptr_t entry = (uintptr_t)syscall_entry;
    wrmsr(MSR_LSTAR, (uint32_t)entry, (uint32_t)(entry >> 32));

    // 3. Set up SFMASK: Which flags to CLEAR on syscall
    // We clear the Interrupt Flag (0x200) to prevent nested interrupts 
    // until we switch to the kernel stack.
    wrmsr(MSR_SFMASK, 0x300, 0);
}
void syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r11, uint64_t rax) {
    // Note: On x86_64 'syscall' puts RIP in RCX and RFLAGS in R11
    
    switch (rax) {
        case 1: // print_char
            debug_putchar((char)rdi);
            break;
            
        case 2: // Example: terminate/exit
            debugln("[SYS] User program exited.");
            hcf();
            break;

        default:
            debugln("[SYS] Unknown syscall: %d", rax);
            break;
    }
}
