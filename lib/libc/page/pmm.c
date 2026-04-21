#include <page.h>
#include <stdint.h>
#include <stdlib.h>

static uint64_t next_free_page = 0;
extern uint64_t hhdm_offset;
extern void hcf(void);

void init_pmm(struct limine_memmap_response* memmap) {
    // Find the biggest entry of type LIMINE_MEMMAP_USABLE
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            // Start allocating from the beginning of this chunk
            next_free_page = entry->base;
	    debugln("Next free physical page: %p", next_free_page);
            return;
        }
    }
}

void* palloc_zero() {
    if (next_free_page == 0) {
        // PMM not initialized or no memory found!
        hcf();
    }
    uint64_t addr = next_free_page;
    next_free_page += 4096; // Move to next page
    
    // Convert physical to virtual via HHDM to zero it
    uint64_t* ptr = (uint64_t*)(addr + hhdm_offset);
    for (int i = 0; i < 512; i++) ptr[i] = 0;
    
    return (void*)addr; // Return PHYSICAL address
}

