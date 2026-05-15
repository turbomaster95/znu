#include <gdt.h>
#include <idt.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <page.h>
#include <syscall.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct tss_entry {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_hi;
    uint32_t base_upper32;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_entry gdt_per_cpu[MAX_CPUS][8];
struct gdt_ptr   gdt_ptr_per_cpu[MAX_CPUS];
struct tss       tss_per_cpu[MAX_CPUS];

cpu_context_t    cpu_contexts[MAX_CPUS];

__attribute__((aligned(16)))
uint8_t ap_kernel_stacks[MAX_CPUS][32768];

void gdt_set_gate_mp(int cpu_id, int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_per_cpu[cpu_id][num].base_low    = (base & 0xFFFF);
    gdt_per_cpu[cpu_id][num].base_middle = (base >> 16) & 0xFF;
    gdt_per_cpu[cpu_id][num].base_high   = (base >> 24) & 0xFF;
    gdt_per_cpu[cpu_id][num].limit_low   = (limit & 0xFFFF);
    gdt_per_cpu[cpu_id][num].granularity = (limit >> 16) & 0x0F;
    gdt_per_cpu[cpu_id][num].granularity |= gran & 0xF0;
    gdt_per_cpu[cpu_id][num].access      = access;
}

void gdt_set_tss_mp(int cpu_id, int num, uint64_t base, uint32_t limit) {
    struct tss_entry *tss = (struct tss_entry *)&gdt_per_cpu[cpu_id][num];

    tss->length       = limit & 0xFFFF;
    tss->base_low     = base & 0xFFFF;
    tss->base_mid     = (base >> 16) & 0xFF;
    tss->flags1       = 0x89; // Present, Executable, TSS descriptor
    tss->flags2       = (limit >> 16) & 0x0F;
    tss->base_hi      = (base >> 24) & 0xFF;
    tss->base_upper32 = (base >> 32) & 0xFFFFFFFF;
    tss->reserved     = 0;
}

void gdt_init_core(int cpu_id) {
    gdt_ptr_per_cpu[cpu_id].limit = (sizeof(struct gdt_entry) * 8) - 1;
    gdt_ptr_per_cpu[cpu_id].base  = (uintptr_t)&gdt_per_cpu[cpu_id];

    gdt_set_gate_mp(cpu_id, 0, 0, 0, 0, 0);                                // Null
    gdt_set_gate_mp(cpu_id, 1, 0, 0xFFFFFFFF, 0x9A, 0xAF); // Kernel Code
    gdt_set_gate_mp(cpu_id, 2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data
    gdt_set_gate_mp(cpu_id, 3, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data
    gdt_set_gate_mp(cpu_id, 4, 0, 0xFFFFFFFF, 0xFA, 0xAF); // User Code

    memset(&tss_per_cpu[cpu_id], 0, sizeof(struct tss));

    tss_per_cpu[cpu_id].rsp0 = (uintptr_t)&ap_kernel_stacks[cpu_id][32768];
    tss_per_cpu[cpu_id].iopb_offset = sizeof(struct tss);

    gdt_set_tss_mp(cpu_id, 5, (uintptr_t)&tss_per_cpu[cpu_id], sizeof(struct tss) - 1);

    asm volatile("lgdt %0" : : "m"(gdt_ptr_per_cpu[cpu_id]));
    
    gdt_reload_segments();
    
    asm volatile("ltr %%ax" : : "a"(0x28));
}
