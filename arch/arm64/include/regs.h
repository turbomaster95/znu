#ifndef REGS_H
#define REGS_H

#include <stdint.h>

typedef struct {
    uint64_t x[31];     // x0-x30
    uint64_t sp;        // Interrupted stack pointer
    uint64_t pc;        // ELR_EL1
    uint64_t pstate;    // SPSR_EL1
    uint64_t esr;       // ESR_EL1
    uint64_t far;       // FAR_EL1
} PACKED registers_t;

#endif /* REGS_H */
