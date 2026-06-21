#ifndef IDT_H
#define IDT_H

#include <stdint.h>

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

typedef struct {
    uint64_t es, ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;

    uint64_t rip;    // Expected at [rsp]
    uint64_t cs;     // Expected at [rsp+8]
    uint64_t rflags; // Expected at [rsp+16]
    uint64_t rsp;    // Expected at [rsp+24]
    uint64_t ss;     // Expected at [rsp+32]
} __attribute__((packed)) registers_t;

void idt_init(void);
void idt_local_load(void);
void idt_global_init(void);
void register_interrupt_handler(uint8_t n, void (*handler)(registers_t*), const char* name);

#endif /* IDT_H */

