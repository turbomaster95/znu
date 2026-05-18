#include <smp.h>
#include <string.h>
#include <lapic.h>
#include <stdlib.h>
#include <page.h>
#include <limine.h>
#include <gdt.h>
#include <idt.h>

extern volatile struct limine_mp_request mp_request;
extern void debug_write(const char* data);
extern bool vmm_ready;
extern uint64_t* vmm_get_kernel_pml4(void);

uint64_t* kernel_pml4_phys = NULL;

void ap_kernel_entry(struct limine_mp_info *info) {
    kernel_pml4_phys = vmm_get_kernel_pml4();

    asm volatile(
        "mov %0, %%cr3" 
        : 
        : "r"(kernel_pml4_phys) 
        : "memory"
    );

    int cpu_id = (int)info->extra_argument;
    gdt_init_core(cpu_id);
    idt_local_load();
    lapic_init_per_core();

    debugln("[smp-core] CPU Core %d is online and ready.", cpu_id);

    while (1) {
	asm volatile(
	    "movq $0xCAFEBABECAFEBABE, %%rax"
    	    : : : "rax"
	);
        asm volatile("cli; hlt");
    }
}

void smp_init(void) {
    struct limine_mp_response *mp_response = mp_request.response;
    if (mp_request.response == NULL) {
        debugln("[smp] Error: Limine MP response not found!");
        return;
    }

    uint64_t cpu_count = mp_response->cpu_count;
    uint32_t bsp_lapic_id = mp_response->bsp_lapic_id;

    debugln("[smp] Discovered %lu CPU cores available.", cpu_count);

    int current_ap_index = 1;

    for (uint64_t i = 0; i < cpu_count; i++) {
        struct limine_mp_info *cpu_info = mp_response->cpus[i];

        if (cpu_info->lapic_id == bsp_lapic_id) {
            continue;
        }

        cpu_info->extra_argument = (uint64_t)current_ap_index;

        debugln("[smp] Booting AP Core (LAPIC ID: %u) with Internal ID: %d...", 
                cpu_info->lapic_id, current_ap_index);

        __atomic_store_n(&cpu_info->goto_address, ap_kernel_entry, __ATOMIC_RELEASE);

        current_ap_index++;
    }

    debugln("[smp] All secondary cores signaled.");
}
