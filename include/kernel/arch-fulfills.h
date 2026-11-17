#ifndef _ARCH_FULFILLS_H
#define _ARCH_FULFILLS_H

void arch_early_init(void);
void arch_interrupt_init(void);
void arch_timer_init(uint32_t frequency);
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);
_Noreturn void arch_halt(void);
void arch_debug_cpu_info(void);
void arch_pat_init(void);
void arch_map_local_irq(uint64_t *pml4);
void arch_seed_entropy(void);
void arch_debug_kernel_pml4(uint64_t *pml4);
void arch_timer_start(void);
void arch_uaccess_enable(void);
bool arch_has_acpi(void);
void arch_mp_init(void);
void arch_syscall_init(uintptr_t stack_top);
void arch_yield(void);
void arch_main_init(void);
void arch_basic_init(void);
_Noreturn void arch_enter_usermode(uint64_t *pml4, uintptr_t kstack_top,
                                   uintptr_t entry, uintptr_t stack);
#endif
