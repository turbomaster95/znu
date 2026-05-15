#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <stddef.h>
#include <uacpi/types.h>
#include <uacpi/tables.h>

#define MAX_CORES 32
#define TRAMPOLINE_PHYS_ADDR 0x8000
#define TRAMPOLINE_VECTOR    0x08

typedef struct {
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} kernel_cpu_core_t;

typedef struct {
    uint64_t stack;
    uint64_t entry;
    uint32_t cr3;
    uint32_t _pad;
} __attribute__((packed, aligned(8))) ap_trampoline_data_t;

extern kernel_cpu_core_t g_cpu_cores[MAX_CORES];
extern size_t g_cpu_core_count;
extern uint32_t g_lapic_physical_address;

void smp_discover_topology(void);
void smp_init(void);
void smp_boot_core(uint8_t apic_id);

#endif
