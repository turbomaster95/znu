#ifndef ARCH_CPU_MRS_H
#define ARCH_CPU_MRS_H

#include <stdint.h>

#define MRS_READ(reg) ({ \
    uint64_t __v; \
    __asm__ volatile("mrs %0, " #reg : "=r"(__v) :: "memory"); \
    __v; \
})

#define MRS_WRITE(reg, val) do { \
    __asm__ volatile("msr " #reg ", %0" :: "r"((uint64_t)(val)) : "memory"); \
} while (0)

static inline void isb(void) {
    __asm__ volatile("isb" ::: "memory");
}

static inline void dsb(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void dmb(void) {
    __asm__ volatile("dmb sy" ::: "memory");
}

static inline uint64_t read_currentel(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_daif(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, DAIF" : "=r"(v) :: "memory");
    return v;
}

static inline void write_daif(uint64_t v) {
    __asm__ volatile("msr DAIF, %0" :: "r"(v) : "memory");
}

static inline void arm64_irq_enable(void) {
    __asm__ volatile("msr DAIFClr, #2" ::: "memory");
}

static inline void arm64_irq_disable(void) {
    __asm__ volatile("msr DAIFSet, #2" ::: "memory");
}

static inline uint64_t arm64_irq_save(void) {
    uint64_t flags = read_daif();
    arm64_irq_disable();
    return flags;
}

static inline void arm64_irq_restore(uint64_t flags) {
    write_daif(flags);
}

static inline uint64_t read_sctlr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_sctlr_el1(uint64_t v) {
    __asm__ volatile("msr SCTLR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_tcr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, TCR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_tcr_el1(uint64_t v) {
    __asm__ volatile("msr TCR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_ttbr0_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, TTBR0_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_ttbr0_el1(uint64_t v) {
    __asm__ volatile("msr TTBR0_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_ttbr1_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, TTBR1_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_ttbr1_el1(uint64_t v) {
    __asm__ volatile("msr TTBR1_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_mair_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, MAIR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_mair_el1(uint64_t v) {
    __asm__ volatile("msr MAIR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_vbar_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, VBAR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_vbar_el1(uint64_t v) {
    __asm__ volatile("msr VBAR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_mpidr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, MPIDR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_tpidr_el0(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, TPIDR_EL0" : "=r"(v) :: "memory");
    return v;
}

static inline void write_tpidr_el0(uint64_t v) {
    __asm__ volatile("msr TPIDR_EL0, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_tpidr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, TPIDR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_tpidr_el1(uint64_t v) {
    __asm__ volatile("msr TPIDR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_cntfrq_el0(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_cntpct_el0(void) {
    uint64_t v;
    __asm__ volatile("isb; mrs %0, CNTPCT_EL0" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_cntvct_el0(void) {
    uint64_t v;
    __asm__ volatile("isb; mrs %0, CNTVCT_EL0" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_cntp_tval_el0(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, CNTP_TVAL_EL0" : "=r"(v) :: "memory");
    return v;
}

static inline void write_cntp_tval_el0(uint64_t v) {
    __asm__ volatile("msr CNTP_TVAL_EL0, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_cntp_ctl_el0(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, CNTP_CTL_EL0" : "=r"(v) :: "memory");
    return v;
}

static inline void write_cntp_ctl_el0(uint64_t v) {
    __asm__ volatile("msr CNTP_CTL_EL0, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_esr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, ESR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_elr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, ELR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_elr_el1(uint64_t v) {
    __asm__ volatile("msr ELR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_spsr_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, SPSR_EL1" : "=r"(v) :: "memory");
    return v;
}

static inline void write_spsr_el1(uint64_t v) {
    __asm__ volatile("msr SPSR_EL1, %0" :: "r"(v) : "memory");
}

static inline uint64_t read_far_el1(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, FAR_EL1" : "=r"(v) :: "memory");
    return v;
}

#endif
