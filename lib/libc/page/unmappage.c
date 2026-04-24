#include <page.h>
#include <stdint.h>
#include <stddef.h>

extern uint64_t hhdm_offset;

void unmap_page(uint64_t* pml4, uint64_t virt) {
    uint64_t pml4_idx = PML4_IDX(virt);
    uint64_t pdp_idx  = PDP_IDX(virt);
    uint64_t pd_idx   = PD_IDX(virt);
    uint64_t pt_idx   = PT_IDX(virt);

    // 1. Traverse to PDP
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return;
    uint64_t* pdp = (uint64_t*)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);

    // 2. Traverse to PD
    if (!(pdp[pdp_idx] & PTE_PRESENT)) return;
    uint64_t* pd = (uint64_t*)((pdp[pdp_idx] & ~0xFFF) + hhdm_offset);

    // 3. Traverse to PT
    if (!(pd[pd_idx] & PTE_PRESENT)) return;
    uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);

    // 4. Clear the Page Table Entry
    // We set it to 0 to mark it as not present and clear all flags/PFN
    pt[pt_idx] = 0;

    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    
}
