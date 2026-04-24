#include <page.h>
#include <stdint.h>
#include <stdlib.h>

extern uint64_t hhdm_offset;

void map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = PML4_IDX(virt);
    uint64_t pdp_idx  = PDP_IDX(virt);
    uint64_t pd_idx   = PD_IDX(virt);
    uint64_t pt_idx   = PT_IDX(virt);

    // 1. PML4 -> PDP
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
       debugln("Allocating new PDP...");
       pml4[pml4_idx] = (uint64_t)palloc_zero() | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdp = (uint64_t*)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);

    // 2. PDP -> PD
    if (!(pdp[pdp_idx] & PTE_PRESENT)) {
        pdp[pdp_idx] = (uint64_t)palloc_zero() | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)((pdp[pdp_idx] & ~0xFFF) + hhdm_offset);

    // 3. PD -> PT
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        pd[pd_idx] = (uint64_t)palloc_zero() | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);

    // 4. PT -> Physical Page
    pt[pt_idx] = (phys & ~0xFFF) | flags | PTE_PRESENT;

    // Flush the TLB for this address
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

