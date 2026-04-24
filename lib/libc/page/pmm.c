#include <page.h>
#include <stdint.h>
#include <string.h> // For memset if you have it, otherwise use a loop
#include <stdlib.h>

static uint8_t* bitmap;
static uint64_t max_pages = 0;
static uint64_t last_index = 0; // Optimization: start searching from last allocated

extern uint64_t hhdm_offset;
extern void hcf(void);

void init_pmm(struct limine_memmap_response* memmap) {
    uint64_t top_address = 0;
    uint64_t biggest_chunk_base = 0;
    uint64_t biggest_chunk_len = 0;

    // 1. Find the total amount of RAM and the biggest chunk to hold the bitmap
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (entry->base + entry->length > top_address)
                top_address = entry->base + entry->length;

            if (entry->length > biggest_chunk_len) {
                biggest_chunk_len = entry->length;
                biggest_chunk_base = entry->base;
            }
        }
    }

    max_pages = top_address / 4096;
    uint64_t bitmap_size = max_pages / 8;

    // 2. Place the bitmap in the biggest usable chunk
    bitmap = (uint8_t*)(biggest_chunk_base + hhdm_offset);
    
    // Mark everything as "Used" (1) initially
    for (uint64_t i = 0; i < bitmap_size; i++) bitmap[i] = 0xFF;

    // 3. Mark only USABLE regions as "Free" (0)
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < entry->length; j += 4096) {
                uint64_t page_idx = (entry->base + j) / 4096;
                bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
            }
        }
    }

    // 4. Protect the bitmap itself! (Mark its pages as used)
    for (uint64_t i = 0; i < bitmap_size; i += 4096) {
        uint64_t page_idx = (biggest_chunk_base / 4096) + (i / 4096);
        bitmap[page_idx / 8] |= (1 << (page_idx % 8));
    }
}

// Internal helper to find a free page
void* palloc() {
    for (uint64_t i = last_index; i < max_pages; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8)); // Mark as used
            last_index = i; 
            return (void*)(i * 4096);
        }
    }
    hcf(); // Out of memory
    return NULL;
}

// Backward compatible wrapper
void* palloc_zero() {
    void* phys_addr = palloc();
    
    // Zero out the page via HHDM
    uint64_t* ptr = (uint64_t*)((uint64_t)phys_addr + hhdm_offset);
    for (int i = 0; i < 512; i++) ptr[i] = 0;

    return phys_addr;
}

// Now you can actually free memory!
void pfree(void* phys_addr) {
    uint64_t page_idx = (uint64_t)phys_addr / 4096;
    bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
    if (page_idx < last_index) last_index = page_idx;
}



void debug_ram_map(struct limine_memmap_response* memmap) {
    debugln("--- RAM MAP ---");
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* en = memmap->entries[i];
        
        // Manual "Suckless" Hex printing if %lx fails
        // Or just print the type and size to verify the PMM
        const char* type = "OTHER";
        if (en->type == 0) type = "USABLE";
        if (en->type == 1) type = "RESERVED";
        if (en->type == 2) type = "ACPI_RECLAIM";
        if (en->type == 4) type = "KERNEL";

        debugln("Type: %d | Size: %d KB", (int)en->type, (int)(en->length / 1024));
        
        // ASCII representation
        if (en->type == 0)      debugln("|  (FREE)  |");
        else if (en->type == 4) debugln("| [KERNEL] |");
        else                    debugln("|XXXXXXXXXX|");
        
        debugln("----------");
    }
}

