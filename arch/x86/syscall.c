#include <stdint.h>
#include <stdlib.h>

extern void hcf(void);
extern void syscall_entry(void);

#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_SFMASK       0xC0000084
#define MSR_GS_BASE      0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102
#define EFER_MSR 0xC0000080
#define EFER_SCE (1 << 0) // System Call Enable

void enable_syscalls() {
    uint32_t low, high;
    // Read current EFER value
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(EFER_MSR));

    // Set the SCE bit
    low |= EFER_SCE;

    // Write it back
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(EFER_MSR));
}

// This structure is what GS points to for stack switching
typedef struct {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
} cpu_context_t;

static cpu_context_t main_cpu_context;

void gs_init(uintptr_t kernel_stack_top) {
    main_cpu_context.kernel_stack = kernel_stack_top;
    main_cpu_context.user_stack_scratch = 0;

    uintptr_t addr = (uintptr_t)&main_cpu_context;

    // Set the GS base so the CPU knows where our context struct is.
    // We set BOTH so that swapgs has a valid target to swap with.
    wrmsr(MSR_GS_BASE, (uint32_t)addr, (uint32_t)(addr >> 32));
    wrmsr(MSR_KERNEL_GS_BASE, (uint32_t)addr, (uint32_t)(addr >> 32));
    
    debugln("[SYS] GS Base initialized to %p", (void*)addr);
}

void syscall_init() {
    uint32_t lo, hi;

    // STAR MSR Configuration
    // [63:48] User Segments: Base for SYSRET. 
    //         SYSRET sets SS = (Base + 8) and CS = (Base + 16)
    //         To get SS=0x18 and CS=0x20, we must set Base to 0x10.
    // [47:32] Kernel Segments: Base for SYSCALL.
    //         SYSCALL sets SS = (Base + 8) and CS = (Base)
    //         To get CS=0x08 and SS=0x10, we set Base to 0x08.
    
    uint32_t kernel_base = 0x08;
    uint32_t user_base = 0x10; // (0x10 + 8 = 0x18 for SS, 0x10 + 16 = 0x20 for CS)

    hi = (user_base << 16) | kernel_base;
    lo = 0;
    wrmsr(MSR_STAR, lo, hi);

    // LSTAR: The 64-bit entry point
    uintptr_t entry = (uintptr_t)syscall_entry;
    wrmsr(MSR_LSTAR, (uint32_t)entry, (uint32_t)(entry >> 32));

    // SFMASK: Bits to clear in RFLAGS on entry.
    // Clear Interrupts (0x200), Trap (0x100), and Direction (0x400)
    wrmsr(MSR_SFMASK, 0x700, 0);

    debugln("[SYS] Syscall MSRs initialized.");
}

void syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9) {
    switch (r9) {
        case 1: // print_char
            debug_putchar((char)rdi);
            break;

        case 2: // exit
            debugln("\n[SYS] User process requested exit.");
            hcf();
            break;

        default:
            debugln("[SYS] Unknown syscall %d from user!", r9);
            break;
    }
}

