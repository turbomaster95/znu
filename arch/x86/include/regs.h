#ifndef REGS_H
#define REGS_H

#include <stdint.h>

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

#endif /* REGS_H */

