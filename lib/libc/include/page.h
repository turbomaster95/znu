#ifndef _PAGE_H
#define _PAGE_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <limine.h>

extern uint64_t hhdm_offset;
extern bool nx_supported;

#define PAGE_SIZE 4096

#define PTE_PRESENT    (1ULL << 0)  // Page is present in memory
#define PTE_WRITABLE   (1ULL << 1)  // Page is writable
#define PTE_USER       (1ULL << 2)  // Page is accessible by Ring 3 (User)
#define PTE_PWT        (1ULL << 3)  // Page-level Write-Through
#define PTE_CACHE_DISABLE    (1ULL << 4)  // Page-level Cache Disable
#define PTE_ACCESSED   (1ULL << 5)  // CPU sets this when accessed
#define PTE_DIRTY      (1ULL << 6)  // CPU sets this when written to
#define PTE_HUGE       (1ULL << 7)  // If set in PD/PDPT, points to 2MB/1GB page
#define PTE_GLOBAL     (1ULL << 8)  // Page is global (ignored in TLB flushes)
#define PTE_NX         (1ULL << 63)

#define PTE_READ_ONLY  (PTE_PRESENT)
#define PTE_READ_WRITE (PTE_PRESENT | PTE_WRITABLE)
#define PTE_USER_DATA  (PTE_PRESENT | PTE_WRITABLE | PTE_USER)
#define PTE_USER_RO    (PTE_PRESENT | PTE_USER)

// Extract indices from a virtual address
#define PML4_IDX(addr) (((uint64_t)(addr) >> 39) & 0x1FF)
#define PDP_IDX(addr)  (((uint64_t)(addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((uint64_t)(addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((uint64_t)(addr) >> 12) & 0x1FF)

#define PHYS_TO_VIRT(addr) ((void*)((uint64_t)(addr) + hhdm_offset))
#define VIRT_TO_PHYS(addr) ((uint64_t)(addr) - hhdm_offset)
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr)   (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define FROM_HHDM(x) (void *)((uintptr_t)x - hhdm_offset)

#if defined(__is_libk)

void  init_pmm(struct limine_memmap_response* memmap);
void* palloc_zero(void);
void  pfree(void* phys_addr);
void *krealloc(void *ptr, size_t new_size);
void  init_slab(void);
void  map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void unmap_page(uint64_t* pml4, uint64_t virt);
void  debug_ram_map(struct limine_memmap_response* memmap);
void* kmalloc(uint64_t size);
void  kfree(void* ptr);
void  vmm_switch(uint64_t* pml4_virt);
void  vmm_map_region(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags);
void  init_vmm(struct limine_memmap_response* memmap);
void* heap_extend(uint64_t pages);
uint64_t* vmm_get_kernel_pml4(void);
uintptr_t vmm_virt_to_phys(uint64_t* pml4, uintptr_t virt);
void vmm_free_user_pages(uint64_t *pml4);
void *kzalloc(size_t size);

#endif
#endif
