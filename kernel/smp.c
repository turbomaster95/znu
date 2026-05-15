#include <smp.h>
#include <string.h>
#include <uacpi/types.h>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <lapic.h>
#include <stdlib.h>
#include <page.h>
#include <tsc.h>
#include <prelude.h>

extern uint8_t _binary_arch_x86_ap_trampoline_bin_start[];
extern uint8_t _binary_arch_x86_ap_trampoline_bin_end[];

extern uint64_t* kernel_pml4;
extern volatile struct limine_hhdm_request hhdm_request;

kernel_cpu_core_t g_cpu_cores[MAX_CORES];
size_t g_cpu_core_count = 0;

// Internal, independent packed definitions to safely map raw ACPI table chunks
typedef struct {
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) local_madt_header_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) local_madt_generic_t;

typedef struct {
    local_madt_generic_t generic;
    uint8_t processor_id;
    uint8_t local_apic_id;
    uint32_t flags;
} __attribute__((packed)) local_madt_lapic_t;

typedef struct {
    local_madt_generic_t generic;
    uint16_t reserved;
    uint32_t local_x2apic_id;
    uint32_t flags;
    uint32_t processor_id;
} __attribute__((packed)) local_madt_x2apic_t;

#define LOCAL_MADT_ENABLED_FLAG 1
#define LOCAL_MADT_TYPE_LAPIC 0
#define LOCAL_MADT_TYPE_X2APIC 9

// Force exact alignment pairing with the end of your NASM padding blocks
typedef struct {
    uint64_t self;              // 0x0FD0
    uint32_t target_stage;      // 0x0FD8
    uint32_t initiator_stage;   // 0x0FDC
    uint32_t pml4;              // 0x0FE0
    uint32_t _pad;              // 0x0FE4
    uint64_t stack;             // 0x0FE8
    uint64_t main;              // 0x0FF0
    uint64_t cpu_context;       // 0x0FF8
} __attribute__((packed)) status_block_t;

_Static_assert(offsetof(status_block_t, self) == 0x00, "bad self");
_Static_assert(offsetof(status_block_t, target_stage) == 0x08, "bad target");
_Static_assert(offsetof(status_block_t, initiator_stage) == 0x0C, "bad init");
_Static_assert(offsetof(status_block_t, pml4) == 0x10, "bad pml4");
_Static_assert(offsetof(status_block_t, stack) == 0x18, "bad stack");
_Static_assert(offsetof(status_block_t, main) == 0x20, "bad main");

#define TRAMPOLINE_PHYS 0x00008000
#define ICR_INIT    0x00000500
#define ICR_STARTUP 0x00000600

void smp_discover_topology(void) {
    uacpi_table madt_tbl;
    if (uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt_tbl) != UACPI_STATUS_OK) {
        debugerr("[smp] MADT not found");
        return;
    }

    g_cpu_core_count = 0;
    
    // Fallback to safely calculating layout structures across raw memory offsets
    size_t offset = sizeof(struct acpi_sdt_hdr) + sizeof(local_madt_header_t);
    
    while (offset < madt_tbl.hdr->length) {
        local_madt_generic_t *g = (local_madt_generic_t *)(madt_tbl.virt_addr + offset);
        if (g->length == 0) break;
        
        if (g->type == LOCAL_MADT_TYPE_LAPIC) {
            local_madt_lapic_t *e = (local_madt_lapic_t *)g;
            if ((e->flags & LOCAL_MADT_ENABLED_FLAG) && g_cpu_core_count < MAX_CORES) {
                g_cpu_cores[g_cpu_core_count].processor_id = e->processor_id;
                g_cpu_cores[g_cpu_core_count].apic_id = e->local_apic_id;
                g_cpu_cores[g_cpu_core_count++].flags = e->flags;
            }
        } else if (g->type == LOCAL_MADT_TYPE_X2APIC) {
            local_madt_x2apic_t *e = (local_madt_x2apic_t *)g;
            if ((e->flags & LOCAL_MADT_ENABLED_FLAG) && g_cpu_core_count < MAX_CORES) {
                g_cpu_cores[g_cpu_core_count].processor_id = e->processor_id;
                g_cpu_cores[g_cpu_core_count].apic_id = e->local_x2apic_id;
                g_cpu_cores[g_cpu_core_count++].flags = e->flags;
            }
        }
        offset += g->length;
    }
    uacpi_table_unref(&madt_tbl);
    debugln("[smp] Found %zu cores", g_cpu_core_count);
}

void ap_kernel_entry(status_block_t *status) {
    // Fix: Reverted to empty parameters to match your arch/x86/lapic.c signature
    lapic_init(); 
    calibrate_lapic_timer_no_irq();

    debugln("[smp-ap] AP up! APIC ID: %u", (uint32_t)(status->self & 0xFF));
    
    __atomic_store_n(&status->target_stage, 2, __ATOMIC_RELEASE);
    
    while (1) {
        __asm__ volatile("hlt");
    }
}

static void early_delay_us(uint64_t us) {
    uint64_t start = tsc_read(); 
    uint64_t ticks = us * 2000; 
    while ((tsc_read() - start) < ticks) {
        __asm__ volatile("pause");
    }
}

void smp_init(void) {
    map_page(kernel_pml4, 0x8000, 0x8000, PTE_PRESENT | PTE_WRITABLE);
    smp_discover_topology();

    if (g_cpu_core_count <= 1) {
        debugln("[smp] Single core detected. Skipping Multi-Processor init.");
        return;
    }

    uintptr_t hhdm_offset = hhdm_request.response->offset;
    uint8_t *trampoline_virt = (uint8_t *)(TRAMPOLINE_PHYS + hhdm_offset);
    size_t trampoline_size = (uintptr_t)_binary_arch_x86_ap_trampoline_bin_end - 
                             (uintptr_t)_binary_arch_x86_ap_trampoline_bin_start;

    debugln("[smp] Copying trampoline to physical destination 0x%x", TRAMPOLINE_PHYS);
    memcpy(trampoline_virt, _binary_arch_x86_ap_trampoline_bin_start, trampoline_size);

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
    uintptr_t bsp_lapic_phys = (lo & 0xFFFFF000);
    volatile uint32_t *bsp_lapic = (volatile uint32_t *)(bsp_lapic_phys + hhdm_offset);
    uint8_t bsp_apic_id = (bsp_lapic[0x20 / 4] >> 24) & 0xFF; 

    volatile status_block_t *status = (volatile status_block_t *)(trampoline_virt + 0x0FD0);
    uint32_t pml4_phys = (uint32_t)((uintptr_t)kernel_pml4 & 0xFFFFFFFF);

    for (size_t i = 0; i < g_cpu_core_count; i++) {
        uint8_t target_apic_id = g_cpu_cores[i].apic_id;

        if (target_apic_id == bsp_apic_id) {
            debugln("[smp] Core index %zu is the active BSP. Skipping boot sequence.", i);
            continue;
        }

        debugln("[smp] Attempting to wake Core APIC ID: %u", target_apic_id);

        void *ap_stack = kmalloc(16384); 
        uint64_t ap_stack_top = (uint64_t)ap_stack + 16384;

        status->self = target_apic_id;
        status->target_stage = 0;
        status->initiator_stage = 1;
        status->pml4 = pml4_phys;
        status->stack = ap_stack_top;
        status->main = (uint64_t)ap_kernel_entry;

        lapic_write(0x280, 0); 

        lapic_send_ipi(target_apic_id, ICR_INIT);
        early_delay_us(10000); 

        lapic_send_ipi(target_apic_id, ICR_STARTUP | TRAMPOLINE_VECTOR);
        early_delay_us(200);   

        lapic_send_ipi(target_apic_id, ICR_STARTUP | TRAMPOLINE_VECTOR);
        early_delay_us(200);

        size_t timeout = 100000;
        bool success = false;
        while (timeout--) {
            if (__atomic_load_n(&status->target_stage, __ATOMIC_ACQUIRE) == 2) {
                success = true;
                break;
            }
            early_delay_us(10);
        }

        if (success) {
            debugln("[smp] Core %u successfully verified online!", target_apic_id);
        } else {
            debugerr("[smp] Core %u failed to respond to STARTUP sequence.", target_apic_id);
            kfree(ap_stack); 
        }
    }
}

