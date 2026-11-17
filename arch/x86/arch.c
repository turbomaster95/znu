#include <stdint.h>
#include <stdlib.h>
#include <x86.h>
#include <gdt.h>
#include <idt.h>
#include <pi.h>
#include <page.h>
#include <lapic.h>
#include <rtc.h>
#include <timekeeper.h>
#include <mp.h>
#include <syscall.h>
#include <arch/syscall.h>
#include <misc.h>

extern void simd_init(void);

void arch_early_init(void) {
    disable_smap();
    debugln("[arch] Disabled SMAP");

    simd_init();
    debugln("[arch] Initialized SIMD");
}

void arch_interrupt_init(void) {
    __asm__ volatile("cli");

    gdt_init_core(0);
    debugln("[gdt] GDT initialized");

    idt_global_init();
    idt_local_load();
    debugln("[idt] IDT initialized.");
}

void arch_timer_init(uint32_t frequency) {
    pit_init(frequency);
}

void arch_pat_init(void) {
    pat_init();
}

void arch_map_local_irq(uint64_t *pml4) {
    map_page(pml4, 0xffff8000fee00000, 0xfee00000,
             PTE_WRITABLE | PTE_CACHE_DISABLE);
}

void arch_seed_entropy(void) {
    seed_from_hardware();
}

void arch_debug_cpu_info(void) {
    uint16_t cs_reg;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs_reg));
    debugln("[kernel] Current CS: 0x%x", cs_reg);
}

void arch_debug_kernel_pml4(uint64_t *pml4) {
    debugln("[kernel_debug] PML4[511] is: %p", (void *)pml4[511]);
}

void arch_timer_start(void) {
    debugln("[lapic] Starting LAPIC periodic timer...");
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x3);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | (1 << 17));
    lapic_write(LAPIC_REG_INITIAL_COUNT, lapic_ticks_per_ms);
    debugln("[lapic] LAPIC timer armed.");
}

void arch_uaccess_enable(void) {
    enable_smap();
    debugln("[smap] Reenabled SMAP");
}

bool arch_has_acpi(void) {
    return true;
}

void arch_smp_init(void) {
#ifdef CONFIG_SMP
    mp_init();
#endif
}

void arch_syscall_init(uintptr_t stack_top) {
    syscall_init();
    gs_init(stack_top);
}

_Noreturn void arch_enter_usermode(uint64_t *pml4, uintptr_t kstack_top,
                                   uintptr_t entry, uintptr_t stack) {
    extern cpu_context_t main_cpu_context;

    vmm_switch(pml4);
    tss_per_cpu[0].rsp0 = kstack_top;
    main_cpu_context.kernel_stack = kstack_top;

    debugln("[kernel] Jumping to Ring 3...");
    jump_to_usermode(entry, stack);
    panic("Init binary exited!!!");
}

void arch_enable_interrupts(void) {
    __asm__ volatile("sti");
}

void arch_disable_interrupts(void) {
    __asm__ volatile("cli");
}

_Noreturn void arch_halt(void) {
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void arch_ret_halt(void) {
    __asm__ volatile("sti; hlt; cli" ::: "memory");
}

void arch_main_init(void) {
    lapic_init(0);
    debugln("[lapic] LAPIC initialized.");

    ps2_init();

    serial_init();
    
    debugln("[lapic] LAPIC calibrated: %u ticks/ms", lapic_ticks_per_ms);

    arch_disable_interrupts();
    pit_init(1000);
    outb(0x21, 0xFD);
    arch_enable_interrupts();

    debugln("[lapic] Starting LAPIC periodic timer...");
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x3);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | (1 << 17));
    lapic_write(LAPIC_REG_INITIAL_COUNT,lapic_ticks_per_ms);
    debugln("[lapic] LAPIC timer armed.");
}

void arch_basic_init(void) {
    debugln("[kernel] Starting uACPI...");

    uacpi_status status = uacpi_initialize(UACPI_LOG_DEBUG);
    if (status != UACPI_STATUS_OK) {
        debugerr("[ERROR] uACPI init failed: %s", uacpi_status_to_string(status));
        arch_halt();
    }
    //debugln("[kernel] uACPI Initialized!");

    status = uacpi_namespace_load();
    if (status != UACPI_STATUS_OK) {
        debugerr("[ERROR] Namespace load failed!");
    }
    //debugln("[kernel] uACPI Namespace Loaded!");

    //debugln("[kernel_debug] About to initialize namespace..");
    status = uacpi_namespace_initialize();
    debugln("[SUCCESS] uACPI is live.");
}

void arch_yield(void) {
   __asm__ volatile("pause" ::: "memory");
}
