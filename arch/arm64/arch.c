#include <stdint.h>
#include <arch/prelude.h>
#include <arch/serial.h>
#include <cpu_mrs.h>

extern void debugln(const char *format, ...);
extern void srand(unsigned int seed);

void arch_early_init(void) {
    arch_serial_init();
    p_info("arch", "PL011 console initialized");
}

void arch_interrupt_init(void) {
    uint64_t cpacr;
    __asm__ volatile("mrs %0, CPACR_EL1" : "=r"(cpacr) :: "memory");
    cpacr |= (3ULL << 20);
    __asm__ volatile("msr CPACR_EL1, %0" :: "r"(cpacr) : "memory");
    isb();

    arm64_irq_disable();
    p_info("arch", "FPU enabled, interrupts masked");
}

void arch_timer_init(uint32_t frequency) {
    (void)frequency;
}

void arch_pat_init(void) {}

void arch_map_local_irq(uint64_t *pml4) {
    (void)pml4;
}

void arch_seed_entropy(void) {
    uint64_t seed = read_cntpct_el0() ^ (read_mpidr_el1() << 32);
    srand((unsigned int)seed);
}

void arch_debug_cpu_info(void) {
    p_debug("arch", "CurrentEL: 0x%llx MPIDR: 0x%llx",
            (unsigned long long)read_currentel(),
            (unsigned long long)read_mpidr_el1());
}

void arch_debug_kernel_pml4(uint64_t *pml4) {
    p_debug("kernel_debug", "L0[511] is: %p", (void *)pml4[511]);
}

void arch_timer_start(void) {
}

void arch_uaccess_enable(void) {
}

bool arch_has_acpi(void) {
    return false;
}

void arch_smp_init(void) {
}

void arch_syscall_init(uintptr_t stack_top) {
    (void)stack_top;
}

_Noreturn void arch_enter_usermode(uint64_t *pml4, uintptr_t kstack_top,
                                   uintptr_t entry, uintptr_t stack) {
    (void)pml4;
    (void)kstack_top;
    p_warn("arch", "Cannot enter userspace yet: entry=%p stack=%p",
            (void *)entry, (void *)stack);
    arch_halt();
}

void arch_enable_interrupts(void) {
    arm64_irq_enable();
}

void arch_disable_interrupts(void) {
    arm64_irq_disable();
}

_Noreturn void arch_halt(void) {
    for (;;) {
        __asm__ volatile("wfi");
    }
}

void arch_yield(void) {
   __asm__ volatile("yield" ::: "memory");
}

void arch_main_init(void) {}
