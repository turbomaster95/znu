#include <page.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <limine.h>

extern uint64_t hhdm_offset;
static uint64_t* kernel_pml4 = NULL;
extern cpu_context_t main_cpu_context;

/**
 * vmm_switch: Activates a PML4 by loading its physical address into CR3
 */
void vmm_switch(uint64_t* pml4_virt) {
    uint64_t phys = VIRT_TO_PHYS(pml4_virt);
    // debugln("[VMM] Switching to PML4 at Phys: %p", phys);
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

uintptr_t vmm_virt_to_phys(uint64_t* pml4, uintptr_t virt) {
    // 1. Extract indices for each level
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;
    uint64_t offset   = virt & 0xFFF;

    // 2. Walk the PML4 to find the PDPT
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(pml4[pml4_idx] & ~0xFFF);

    // 3. Walk the PDPT to find the Page Directory
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;
    // Check if it's a 1GB huge page (Bit 7: PS)
    if (pdpt[pdpt_idx] & (1ULL << 7)) {
        return (pdpt[pdpt_idx] & ~0x3FFFFFFF) + (virt & 0x3FFFFFFF);
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[pdpt_idx] & ~0xFFF);

    // 4. Walk the Page Directory to find the Page Table
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;
    // Check if it's a 2MB large page (Bit 7: PS)
    if (pd[pd_idx] & (1ULL << 7)) {
        return (pd[pd_idx] & ~0x1FFFFF) + (virt & 0x1FFFFF);
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_idx] & ~0xFFF);

    // 5. Walk the Page Table to find the Physical Frame
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;
    uintptr_t phys = (pt[pt_idx] & ~0xFFF);

    return phys + offset;
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

    extern void syscall_entry(void);
    uintptr_t entry_phys = vmm_virt_to_phys(boot_pml4, (uintptr_t)syscall_entry);
    uintptr_t ctx_phys = vmm_virt_to_phys(boot_pml4, (uintptr_t)&main_cpu_context);

    extern void syscall_handler(uint64_t);
    uintptr_t handler_phys = vmm_virt_to_phys(boot_pml4, (uintptr_t)syscall_handler);

    uintptr_t stack_phys = vmm_virt_to_phys(boot_pml4, (uintptr_t)main_cpu_context.kernel_stack - 8);

    map_page(kernel_pml4, 
         PAGE_ALIGN_DOWN(main_cpu_context.kernel_stack - 8), 
         PAGE_ALIGN_DOWN(stack_phys), 
         PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    debugln("[VMM] Explicitly mapped kernel stack at %p (Phys: %p)", 
        main_cpu_context.kernel_stack, stack_phys);

    map_page(kernel_pml4, (uintptr_t)syscall_handler, handler_phys, PTE_PRESENT | PTE_USER);
    map_page(kernel_pml4, (uintptr_t)syscall_entry, entry_phys, PTE_PRESENT | PTE_USER);
    map_page(kernel_pml4, (uintptr_t)&main_cpu_context, ctx_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    // uintptr_t stack_phys = vmm_virt_to_phys(boot_pml4, stack_top_address);
    // map_page(kernel_pml4, stack_top_address, stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);

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
 * vmm_clone_pml4: Creates a deep copy of a PML4 (user-space only)
 */
uint64_t* vmm_clone_pml4(uint64_t* src_pml4_virt) {
    // 1. Allocate new PML4
    uint64_t* dst_pml4 = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
    if (!dst_pml4) return NULL;

    // 2. Clone Kernel Half (Entries 256-511)
    // We can just copy these directly as they are shared
    for (int i = 256; i < 512; i++) {
        dst_pml4[i] = src_pml4_virt[i];
    }

    // 3. Clone User Half (Entries 0-255)
    // We need to recursively copy all present pages
    for (int i = 0; i < 256; i++) {
        if (!(src_pml4_virt[i] & PTE_PRESENT)) continue;

        uint64_t* src_pdpt = (uint64_t*)PHYS_TO_VIRT(src_pml4_virt[i] & ~0xFFF);
        uint64_t* dst_pdpt = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
        dst_pml4[i] = VIRT_TO_PHYS(dst_pdpt) | (src_pml4_virt[i] & 0xFFF);

        for (int j = 0; j < 512; j++) {
            if (!(src_pdpt[j] & PTE_PRESENT)) continue;
            
            // Handle 1GB huge pages if necessary, but Znu likely doesn't use them for user space
            if (src_pdpt[j] & (1ULL << 7)) {
                // For now, we don't support cloning huge pages (rare in simple kernels)
                continue;
            }

            uint64_t* src_pd = (uint64_t*)PHYS_TO_VIRT(src_pdpt[j] & ~0xFFF);
            uint64_t* dst_pd = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
            dst_pdpt[j] = VIRT_TO_PHYS(dst_pd) | (src_pdpt[j] & 0xFFF);

            for (int k = 0; k < 512; k++) {
                if (!(src_pd[k] & PTE_PRESENT)) continue;

                if (src_pd[k] & (1ULL << 7)) { // 2MB page
                     continue;
                }

                uint64_t* src_pt = (uint64_t*)PHYS_TO_VIRT(src_pd[k] & ~0xFFF);
                uint64_t* dst_pt = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
                dst_pd[k] = VIRT_TO_PHYS(dst_pt) | (src_pd[k] & 0xFFF);

                for (int l = 0; l < 512; l++) {
                    if (!(src_pt[l] & PTE_PRESENT)) continue;

                    // This is a leaf page. Deep copy it.
                    uint64_t src_phys = src_pt[l] & ~0xFFF;
                    uint64_t* dst_page_virt = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
                    
                    // Copy the content
                    memcpy(dst_page_virt, PHYS_TO_VIRT(src_phys), PAGE_SIZE);

                    // Map it in the new table
                    dst_pt[l] = VIRT_TO_PHYS(dst_page_virt) | (src_pt[l] & 0xFFF);
                }
            }
        }
    }

    return dst_pml4;
}

/**
 * vmm_get_kernel_pml4: Returns the active kernel address space
 */
uint64_t* vmm_get_kernel_pml4(void) {
    return kernel_pml4;
}

