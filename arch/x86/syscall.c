#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>

extern void hcf(void);
extern void syscall_entry(void);

#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_SFMASK       0xC0000084
#define MSR_GS_BASE      0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102
#define EFER_MSR 0xC0000080
#define EFER_SCE (1 << 0) // System Call Enable

static inline uint64_t read_rsp() {
    uint64_t rsp;
    asm volatile("mov %%rsp, %0" : "=r"(rsp));
    return rsp;
}

void enable_syscalls() {
    uint32_t low, high;
    // Read current EFER value
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(EFER_MSR));

    // Set the SCE bit
    low |= EFER_SCE;

    // Write it back
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(EFER_MSR));
}


cpu_context_t main_cpu_context;

void gs_init(uintptr_t kernel_stack_top) {
    main_cpu_context.kernel_stack = kernel_stack_top;
    main_cpu_context.user_stack_scratch = 0;

    uintptr_t addr = (uintptr_t)&main_cpu_context;

    // IMPORTANT: In Ring 0 (now), GS_BASE is active.
    // But for SYSCALL from Ring 3, we need the address in KERNEL_GS_BASE
    // so that 'swapgs' can pull it into the active slot.
    
    wrmsr(MSR_KERNEL_GS_BASE, (uint32_t)addr, (uint32_t)(addr >> 32));
    
    // Set active GS to 0 for now, so we know swapgs actually does something later
    wrmsr(MSR_GS_BASE, 0, 0); 

    debugln("[SYS] GS Shadow initialized to %p", (void*)addr);
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

void syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2) {
    debugln("[SYS] Handler reached, Number: %d, Arg1: %c", num, (char)arg1);
    
    switch (num) {
        case 1: // print_char
            debugln("[USER] %c", (char)arg1); 
            break;
        case 2:
            hcf();
            break;
        default:
            debugln("[SYS] Unknown syscall %d", num);
            break;
    }
}

