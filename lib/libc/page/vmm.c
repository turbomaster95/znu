#include <page.h>
#include <mmu.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <limine.h>

extern uint64_t hhdm_offset;
static pagetable_t kernel_pagetable = 0;
extern cpu_context_t main_cpu_context;

void vmm_switch(pagetable_t table) {
    arch_mmu_switch(table);
}

void vmm_map_region(pagetable_t table, uintptr_t virt, uintptr_t phys, size_t size, uint64_t flags) {
    debugln("[VMM] Mapping region: %p -> %p (Size: %d KB)", virt, phys, (int)(size / 1024));
    
    size_t offset = 0;
    while (offset < size) {
        uintptr_t curr_virt = virt + offset;
        uintptr_t curr_phys = phys + offset;
        size_t remaining = size - offset;

        // Use 2MB huge pages when alignment and size allow
        if ((curr_virt % 0x200000 == 0) && 
            (curr_phys % 0x200000 == 0) && 
            (remaining >= 0x200000)) {
            
            arch_mmu_map_page(table, curr_virt, curr_phys, flags | MMU_MAP_HUGE);
            offset += 0x200000;
        } else {
            arch_mmu_map_page(table, curr_virt, curr_phys, flags);
            offset += PAGE_SIZE;
        }
    }
    
    debugln("[VMM] Region mapping complete.");
}

uintptr_t vmm_virt_to_phys(pagetable_t table, uintptr_t virt) {
    return arch_mmu_virt_to_phys(table, virt);
}

void init_vmm(struct limine_memmap_response* memmap) {
    debugln("[VMM] Initializing Virtual Memory Manager...");

    kernel_pagetable = (pagetable_t)PHYS_TO_VIRT(palloc_zero());
    debugln("[VMM] New kernel pagetable allocated at %p", kernel_pagetable);

    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    pagetable_t boot_table = (pagetable_t)PHYS_TO_VIRT(current_cr3);

    uint64_t* src = (uint64_t*)boot_table;
    uint64_t* dst = (uint64_t*)kernel_pagetable;
    for (int i = 256; i < 512; i++) {
        dst[i] = src[i];
    }

    extern void syscall_entry(void);
    extern void syscall_handler(uint64_t);

    uintptr_t entry_phys   = vmm_virt_to_phys(boot_table, (uintptr_t)syscall_entry);
    uintptr_t ctx_phys     = vmm_virt_to_phys(boot_table, (uintptr_t)&main_cpu_context);
    uintptr_t handler_phys = vmm_virt_to_phys(boot_table, (uintptr_t)syscall_handler);
    uintptr_t stack_phys   = vmm_virt_to_phys(boot_table, (uintptr_t)main_cpu_context.kernel_stack - 8);

    arch_mmu_map_page(kernel_pagetable, PAGE_ALIGN_DOWN(main_cpu_context.kernel_stack - 8), PAGE_ALIGN_DOWN(stack_phys), MMU_MAP_WRITE);
    arch_mmu_map_page(kernel_pagetable, (uintptr_t)syscall_handler, handler_phys, MMU_MAP_READ | MMU_MAP_EXEC);
    arch_mmu_map_page(kernel_pagetable, (uintptr_t)syscall_entry, entry_phys, MMU_MAP_READ | MMU_MAP_EXEC);
    arch_mmu_map_page(kernel_pagetable, (uintptr_t)&main_cpu_context, ctx_phys, MMU_MAP_WRITE);

    arch_mmu_map_page(kernel_pagetable, hhdm_offset, 0, MMU_MAP_WRITE);

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        vmm_map_region(
            kernel_pagetable, 
            entry->base + hhdm_offset, 
            entry->base, 
            PAGE_ALIGN_UP(entry->length), 
            MMU_MAP_WRITE
        );
    }

    // MMIO Mapping
    vmm_map_region(
        kernel_pagetable,
        0xfd000000 + hhdm_offset,
        0xfd000000,
        0x03000000,
        MMU_MAP_WRITE | MMU_MAP_NOCACHE
    );

    vmm_switch(kernel_pagetable);
    debugln("[VMM] VMM initialization successful.");
}

pagetable_t vmm_clone_pml4(pagetable_t src_table) {
    return arch_mmu_clone_user_space(src_table);
}

pagetable_t vmm_get_kernel_pml4(void) {
    return kernel_pagetable;
}

void vmm_free_user_pages(pagetable_t table) {
    arch_mmu_free_user_space(table);
}
