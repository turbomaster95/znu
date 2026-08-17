#include <page.h>
#include <mmu.h>
#include <stdint.h>
#include <stdbool.h>

void map_page(pagetable_t pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t mmu_flags = 0;

    if (flags & MMU_MAP_READ)  mmu_flags |= MMU_MAP_READ;
    if (flags & MMU_MAP_WRITE) mmu_flags |= MMU_MAP_WRITE;
    if (flags & MMU_MAP_EXEC)  mmu_flags |= MMU_MAP_EXEC;
    if (flags & MMU_MAP_USER)  mmu_flags |= MMU_MAP_USER;
    if (flags & MMU_MAP_NOCACHE) mmu_flags |= MMU_MAP_NOCACHE;

    arch_mmu_map_page(pml4, (uintptr_t)virt, (uintptr_t)phys, mmu_flags);
}

void map_page_huge(pagetable_t pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t mmu_flags = MMU_MAP_HUGE;

    if (flags & MMU_MAP_READ)  mmu_flags |= MMU_MAP_READ;
    if (flags & MMU_MAP_WRITE) mmu_flags |= MMU_MAP_WRITE;
    if (flags & MMU_MAP_EXEC)  mmu_flags |= MMU_MAP_EXEC;
    if (flags & MMU_MAP_USER)  mmu_flags |= MMU_MAP_USER;
    if (flags & MMU_MAP_NOCACHE) mmu_flags |= MMU_MAP_NOCACHE;

    arch_mmu_map_page(pml4, (uintptr_t)virt, (uintptr_t)phys, mmu_flags);
}
