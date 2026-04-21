#ifndef _PAGE_H
#define _PAGE_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>
#include <limine.h>

#define PAGE_SIZE 4096
#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_CACHE_DISABLE (1ULL << 4)

// Extract indices from a virtual address
#define PML4_IDX(addr) (((addr) >> 39) & 0x1FF)
#define PDP_IDX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((addr) >> 12) & 0x1FF)


#if defined(__is_libk)

void init_pmm(struct limine_memmap_response* memmap);
void* palloc_zero(void);
void map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);

#endif
#endif
