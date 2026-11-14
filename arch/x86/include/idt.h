#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <regs.h>

struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init(void);
void idt_local_load(void);
void idt_global_init(void);
void register_interrupt_handler(uint8_t n, void (*handler)(registers_t*), const char* name);

#endif /* IDT_H */

