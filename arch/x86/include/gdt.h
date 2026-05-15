#ifndef _GDT_H
#define _GDT_H

#include <stdint.h>

#define MAX_CPUS 32

void gdt_reload_segments(void);
void gdt_set_gate_mp(int cpu_id, int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
void gdt_set_tss_mp(int cpu_id, int num, uint64_t base, uint32_t limit);
void gdt_init_core(int cpu_id);

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

extern struct tss tss_per_cpu[MAX_CPUS];

#endif
