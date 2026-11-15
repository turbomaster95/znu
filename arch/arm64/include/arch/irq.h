#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#include <stdint.h>

void arch_irq_init(void);
uint64_t arch_irq_save(void);
void arch_irq_restore(uint64_t flags);
void arch_idle(void);

#endif
