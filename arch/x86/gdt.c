#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <page.h>

extern void gdt_reload_segments(void);

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

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;      // Stack pointer for Ring 0
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

struct gdt_entry gdt[8];
struct gdt_ptr gdt_ptr;
struct tss kernel_tss;

__attribute__((aligned(16)))
uint8_t kernel_stack[16384];

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void gdt_set_tss(int num, uint64_t base, uint32_t limit) {
    struct tss_entry *tss = (struct tss_entry *)&gdt[num];

    tss->length       = limit & 0xFFFF;
    tss->base_low     = base & 0xFFFF;
    tss->base_mid     = (base >> 16) & 0xFF;
    tss->flags1       = 0x89; // Present, Type: 64-bit TSS (Available)
    tss->flags2       = (limit >> 16) & 0x0F;
    tss->base_hi      = (base >> 24) & 0xFF;
    tss->base_upper32 = (base >> 32) & 0xFFFFFFFF;
    tss->reserved     = 0;
}

void gdt_init() {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uintptr_t)&gdt;

    debugln("[gdt] Initializing..");
    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF); // Kernel Code (0x08)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data (0x10)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data   (0x18)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xFA, 0xAF); // User Code   (0x20)

    for(int i=0; i<sizeof(struct tss); i++) ((uint8_t*)&kernel_tss)[i] = 0;

    kernel_tss.rsp0 = (uintptr_t)kernel_stack + sizeof(kernel_stack);
    kernel_tss.iopb_offset = sizeof(struct tss);
    gdt_set_tss(5, (uintptr_t)&kernel_tss, sizeof(struct tss) - 1);
    debugln("[gdt] TSS RSP0 set to dedicated stack at: %p", (void*)kernel_tss.rsp0);
    debugln("[gdt_tss] Setup TSS Done!");

    asm volatile("lgdt %0" : : "m"(gdt_ptr));
    gdt_reload_segments();
    asm volatile("ltr %%ax" : : "a"(0x28));
    debugln("[gdt] Initialized!");
}
