#ifndef ARCH_PRELUDE
#define ARCH_PRELUDE

#define _uaccess_begin()
#define _uaccess_end()

void arch_early_init(void);
void arch_interrupt_init(void);
void arch_timer_init(uint32_t frequency);
void arch_enable_interrupts(void);
void arch_disable_interrupts(void);
_Noreturn void arch_halt(void);
void arch_debug_cpu_info(void);

#endif
