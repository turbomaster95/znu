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
    uint64_t es, ds;        // Pushed last in isr_stub (Lowest addresses)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax; // rax was pushed first by pushaq
    uint64_t int_no, err_code;    // Pushed by isr_wrapper
    uint64_t rip, cs, rflags, rsp, ss; // Pushed by CPU (Highest addresses)
} __attribute__((packed)) registers_t;


void idt_init(void);

#endif /* IDT_H */

