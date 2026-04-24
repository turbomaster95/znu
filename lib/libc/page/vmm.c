#include <page.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limine.h>

extern uint64_t hhdm_offset;
static uint64_t* kernel_pml4 = NULL;

/**
 * vmm_switch: Activates a PML4 by loading its physical address into CR3
 */
void vmm_switch(uint64_t* pml4_virt) {
    uint64_t phys = VIRT_TO_PHYS(pml4_virt);
    debugln("[VMM] Switching to PML4 at Phys: %p", phys);
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

/**
 * vmm_map_region: Helper to map a contiguous range of memory
 */
void vmm_map_region(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
    debugln("[VMM] Mapping region: %p -> %p (Size: %d KB)", virt, phys, (int)(size / 1024));
    
    for (uint64_t offset = 0; offset < size; offset += PAGE_SIZE) {
        // CALL map_page WITHOUT individual debugln inside it
        map_page(pml4, virt + offset, phys + offset, flags);
    }
    
    debugln("[VMM] Region mapping complete.");
}


/**
 * init_vmm: Initializes the kernel's virtual memory space
 */
void init_vmm(struct limine_memmap_response* memmap) {
    debugln("[VMM] Initializing Virtual Memory Manager...");

    // 1. Allocate the new PML4
    // We use palloc_zero to ensure the lower-half (user space) is empty
    kernel_pml4 = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
    debugln("[VMM] New kernel PML4 allocated at %p", kernel_pml4);

    // 2. Clone the Bootloader's Higher-Half
    // This is the "Magic Fix" for the Triple Fault. We copy the existing 
    // mappings for the Kernel and HHDM from the bootloader tables.
    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    
    uint64_t* boot_pml4 = (uint64_t*)PHYS_TO_VIRT(current_cr3);
    debugln("[VMM] Cloning bootloader higher-half from %p", boot_pml4);

    // Entries 256-511 represent the top half of the address space (Kernel land)
    for (int i = 256; i < 512; i++) {
        if (boot_pml4[i] != 0) {
            kernel_pml4[i] = boot_pml4[i];
        }
    }

    // 3. Re-map the HHDM based on the Memory Map
    // This ensures our new PML4 has full coverage of all physical RAM chunks
    debugln("[VMM] Mapping HHDM regions into new table...");
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        vmm_map_region(
            kernel_pml4, 
            entry->base + hhdm_offset, 
            entry->base, 
            PAGE_ALIGN_UP(entry->length), 
            PTE_WRITABLE
        );
    }

    debugln("[VMM] Mapping common MMIO range (0xFD000000 - 0xFFFFFFFF)...");
    vmm_map_region(
        kernel_pml4,
        0xfd000000 + hhdm_offset,
        0xfd000000,
        0x03000000, // 48MB covering HPET, LAPIC, and I/O APICs
        PTE_WRITABLE | PTE_CACHE_DISABLE // Cache disable is safer for MMIO
    );

    // 4. Activate the new table
    // After this instruction, the CPU is officially using your kernel_pml4
    vmm_switch(kernel_pml4);
    
    debugln("[VMM] VMM initialization successful. Context switched.");
}

/**
 * vmm_get_kernel_pml4: Returns the active kernel address space
 */
uint64_t* vmm_get_kernel_pml4(void) {
    return kernel_pml4;
}

