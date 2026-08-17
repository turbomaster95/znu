#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096ULL
#endif

#define MMU_MAP_READ      (1ULL << 0)
#define MMU_MAP_WRITE     (1ULL << 1)
#define MMU_MAP_EXEC      (1ULL << 2)
#define MMU_MAP_USER      (1ULL << 3)
#define MMU_MAP_NOCACHE   (1ULL << 4)
#define MMU_MAP_HUGE      (1ULL << 5)

typedef uintptr_t pagetable_t;

void arch_mmu_switch(pagetable_t table);
void arch_mmu_map_page(pagetable_t table, uintptr_t virt, uintptr_t phys, uint64_t flags);
void arch_mmu_unmap_page(pagetable_t table, uintptr_t virt);
uintptr_t arch_mmu_virt_to_phys(pagetable_t table, uintptr_t virt);
pagetable_t arch_mmu_clone_user_space(pagetable_t src_table);
void arch_mmu_free_user_space(pagetable_t table);
void arch_mmu_invlpg(uintptr_t virt);

#endif // MMU_H
