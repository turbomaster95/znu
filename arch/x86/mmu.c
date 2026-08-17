#include <mmu.h>
#include <page.h>
#include <string.h>

extern uint64_t hhdm_offset;
extern bool nx_supported;

static inline uint64_t translate_flags(uint64_t flags) {
    uint64_t pte_flags = PTE_PRESENT;
    if (flags & MMU_MAP_WRITE) pte_flags |= PTE_WRITABLE;
    if (flags & MMU_MAP_USER)  pte_flags |= PTE_USER;
    if (flags & MMU_MAP_NOCACHE) pte_flags |= PTE_PCD;
    if (nx_supported && !(flags & MMU_MAP_EXEC)) pte_flags |= PTE_NX;
    return pte_flags;
}

void arch_mmu_invlpg(uintptr_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void arch_mmu_switch(pagetable_t table) {
    uint64_t phys = VIRT_TO_PHYS(table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

void arch_mmu_map_page(pagetable_t table, uintptr_t virt, uintptr_t phys, uint64_t flags) {
    uint64_t* pml4 = (uint64_t*)table;
    uint64_t pte_flags = translate_flags(flags);

    uint64_t pml4_i = PML4_IDX(virt);
    uint64_t pdp_i  = PDP_IDX(virt);
    uint64_t pd_i   = PD_IDX(virt);
    uint64_t pt_i   = PT_IDX(virt);

    if (!(pml4[pml4_i] & PTE_PRESENT)) {
        uintptr_t pdpt_phys = (uintptr_t)palloc_zero(); // Already physical
        pml4[pml4_i] = pdpt_phys | PTE_PRESENT | PTE_WRITABLE | (pte_flags & PTE_USER);
    } else {
        pml4[pml4_i] |= (pte_flags & PTE_USER);
    }
    uint64_t* pdp = (uint64_t*)PHYS_TO_VIRT(pml4[pml4_i] & ~0xFFF);

    if (!(pdp[pdp_i] & PTE_PRESENT)) {
        uintptr_t pd_phys = (uintptr_t)palloc_zero(); // Already physical
        pdp[pdp_i] = pd_phys | PTE_PRESENT | PTE_WRITABLE | (pte_flags & PTE_USER);
    } else {
        pdp[pdp_i] |= (pte_flags & PTE_USER);
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdp[pdp_i] & ~0xFFF);

    if (flags & MMU_MAP_HUGE) { // 2MB Leaf Mapping
        pd[pd_i] = (phys & ~0x1FFFFF) | pte_flags | PTE_PS;
        arch_mmu_invlpg(virt);
        return;
    }

    if (!(pd[pd_i] & PTE_PRESENT)) {
        uintptr_t pt_phys = (uintptr_t)palloc_zero(); // Already physical
        pd[pd_i] = pt_phys | PTE_PRESENT | PTE_WRITABLE | (pte_flags & PTE_USER);
    } else {
        pd[pd_i] |= (pte_flags & PTE_USER);
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_i] & ~0xFFF);

    pt[pt_i] = (phys & ~0xFFF) | pte_flags;
    arch_mmu_invlpg(virt);
}

void arch_mmu_unmap_page(pagetable_t table, uintptr_t virt) {
    uint64_t* pml4 = (uint64_t*)table;

    if (!(pml4[PML4_IDX(virt)] & PTE_PRESENT)) return;
    uint64_t* pdp = (uint64_t*)PHYS_TO_VIRT(pml4[PML4_IDX(virt)] & ~0xFFF);

    if (!(pdp[PDP_IDX(virt)] & PTE_PRESENT)) return;
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdp[PDP_IDX(virt)] & ~0xFFF);

    if (!(pd[PD_IDX(virt)] & PTE_PRESENT)) return;
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[PD_IDX(virt)] & ~0xFFF);

    pt[PT_IDX(virt)] = 0;
    arch_mmu_invlpg(virt);
}

uintptr_t arch_mmu_virt_to_phys(pagetable_t table, uintptr_t virt) {
    uint64_t* pml4 = (uint64_t*)table;

    if (!(pml4[PML4_IDX(virt)] & PTE_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(pml4[PML4_IDX(virt)] & ~0xFFF);

    if (!(pdpt[PDP_IDX(virt)] & PTE_PRESENT)) return 0;
    if (pdpt[PDP_IDX(virt)] & PTE_PS) {
        return (pdpt[PDP_IDX(virt)] & ~0x3FFFFFFF) + (virt & 0x3FFFFFFF);
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[PDP_IDX(virt)] & ~0xFFF);

    if (!(pd[PD_IDX(virt)] & PTE_PRESENT)) return 0;
    if (pd[PD_IDX(virt)] & PTE_PS) {
        return (pd[PD_IDX(virt)] & ~0x1FFFFF) + (virt & 0x1FFFFF);
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[PD_IDX(virt)] & ~0xFFF);

    if (!(pt[PT_IDX(virt)] & PTE_PRESENT)) return 0;
    return (pt[PT_IDX(virt)] & ~0xFFF) + (virt & 0xFFF);
}

pagetable_t arch_mmu_clone_user_space(pagetable_t src_table) {
    uint64_t* src_pml4 = (uint64_t*)src_table;
    uint64_t* dst_pml4 = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
    if (!dst_pml4) return 0;

    // Kernel-space region share
    for (int i = 256; i < 512; i++) {
        dst_pml4[i] = src_pml4[i];
    }

    // Deep copy User-space entries
    for (int i = 0; i < 256; i++) {
        if (!(src_pml4[i] & PTE_PRESENT)) continue;

        uint64_t* src_pdpt = (uint64_t*)PHYS_TO_VIRT(src_pml4[i] & ~0xFFF);
        uint64_t* dst_pdpt = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
        dst_pml4[i] = VIRT_TO_PHYS(dst_pdpt) | (src_pml4[i] & 0xFFF);

        for (int j = 0; j < 512; j++) {
            if (!(src_pdpt[j] & PTE_PRESENT) || (src_pdpt[j] & PTE_PS)) continue;

            uint64_t* src_pd = (uint64_t*)PHYS_TO_VIRT(src_pdpt[j] & ~0xFFF);
            uint64_t* dst_pd = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
            dst_pdpt[j] = VIRT_TO_PHYS(dst_pd) | (src_pdpt[j] & 0xFFF);

            for (int k = 0; k < 512; k++) {
                if (!(src_pd[k] & PTE_PRESENT) || (src_pd[k] & PTE_PS)) continue;

                uint64_t* src_pt = (uint64_t*)PHYS_TO_VIRT(src_pd[k] & ~0xFFF);
                uint64_t* dst_pt = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
                dst_pd[k] = VIRT_TO_PHYS(dst_pt) | (src_pd[k] & 0xFFF);

                for (int l = 0; l < 512; l++) {
                    if (!(src_pt[l] & PTE_PRESENT)) continue;

                    uint64_t src_phys = src_pt[l] & ~0xFFF;
                    uint64_t* dst_page_virt = (uint64_t*)PHYS_TO_VIRT(palloc_zero());
                    memcpy(dst_page_virt, (void*)PHYS_TO_VIRT(src_phys), PAGE_SIZE);

                    dst_pt[l] = VIRT_TO_PHYS(dst_page_virt) | (src_pt[l] & 0xFFF);
                }
            }
        }
    }
    return (pagetable_t)dst_pml4;
}

void arch_mmu_free_user_space(pagetable_t table) {
    uint64_t* pml4 = (uint64_t*)table;
    if (!pml4) return;

    for (int i = 0; i < 256; i++) {
        if (!(pml4[i] & PTE_PRESENT)) continue;

        uint64_t pdpt_phys = pml4[i] & ~0xFFF;
        uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(pdpt_phys);

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_PRESENT) || (pdpt[j] & PTE_PS)) continue;

            uint64_t pd_phys = pdpt[j] & ~0xFFF;
            uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pd_phys);

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_PRESENT) || (pd[k] & PTE_PS)) continue;

                uint64_t pt_phys = pd[k] & ~0xFFF;
                uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pt_phys);

                for (int l = 0; l < 512; l++) {
                    if (!(pt[l] & PTE_PRESENT)) continue;
                    pfree((void*)(pt[l] & ~0xFFF));
                    pt[l] = 0;
                }
                pfree((void*)pt_phys);
                pd[k] = 0;
            }
            pfree((void*)pd_phys);
            pdpt[j] = 0;
        }
        pfree((void*)pdpt_phys);
        pml4[i] = 0;
    }
}
