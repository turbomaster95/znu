#include <page.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limine.h>
#define BUDDY_ALLOC_IMPLEMENTATION
#include <buddy_alloc.h>

extern uint64_t hhdm_offset;
extern void hcf(void);

static struct buddy* pmm_buddy = NULL;
static uint64_t total_system_pages = 0;
static uint64_t arena_phys_base = 0;
static uint64_t arena_total_len = 0;

void init_pmm(struct limine_memmap_response* memmap) {
    uint64_t top_address = 0;
    uint64_t biggest_chunk_base = 0;
    uint64_t biggest_chunk_len = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (entry->base + entry->length > top_address) {
                top_address = entry->base + entry->length;
            }
            if (entry->length > biggest_chunk_len) {
                biggest_chunk_len = entry->length;
                biggest_chunk_base = entry->base;
            }
        }
    }

    total_system_pages = top_address / 4096;

    if (biggest_chunk_len == 0) {
        hcf(); // Out of memory before initialization
    }

    size_t required_metadata_bytes = buddy_sizeof(biggest_chunk_len);
    
    required_metadata_bytes = (required_metadata_bytes + 4095) & ~0xFFF;

    void* metadata_virt = (void*)(biggest_chunk_base + hhdm_offset);
    
    arena_phys_base = biggest_chunk_base + required_metadata_bytes;
    arena_total_len  = biggest_chunk_len - required_metadata_bytes;
    void* arena_virt = (void*)(arena_phys_base + hhdm_offset);

    pmm_buddy = buddy_init(metadata_virt, arena_virt, arena_total_len);
    
    if (!pmm_buddy) {
        hcf(); // Setup failed
    }
}

void* palloc() {
    if (!pmm_buddy) return NULL;

    // Request a 4096-byte slot block size
    void* allocated_virt = buddy_malloc(pmm_buddy, 4096);
    if (!allocated_virt) {
        hcf(); // STOP and die when completely dry on phisical space
        return NULL;
    }

    return (void*)((uint64_t)allocated_virt - hhdm_offset);
}

void* palloc_contig(size_t size) {
    if (!pmm_buddy) return NULL;
    
    void* allocated_virt = buddy_malloc(pmm_buddy, size);
    if (!allocated_virt) return NULL;

    return (void*)((uint64_t)allocated_virt - hhdm_offset);
}

void* palloc_zero() {
    void* phys_addr = palloc();
    if (!phys_addr) return NULL;
    
    uint64_t* ptr = (uint64_t*)((uint64_t)phys_addr + hhdm_offset);
    for (int i = 0; i < 512; i++) {
        ptr[i] = 0;
    }

    return phys_addr;
}

void pfree(void* phys_addr) {
    if (!phys_addr || !pmm_buddy) return;

    void* virt_token = (void*)((uint64_t)phys_addr + hhdm_offset);
    buddy_free(pmm_buddy, virt_token);
}

void debug_ram_map(struct limine_memmap_response* memmap) {
    debugln("--- RAM MAP ---");
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* en = memmap->entries[i];
        
        debugln("Type: %d | Size: %d KB", (int)en->type, (int)(en->length / 1024));
        
        if (en->type == 0)      debugln("|  (FREE)  |");
        else if (en->type == 4) debugln("| [KERNEL] |");
        else                    debugln("|XXXXXXXXXX|");
        
        debugln("----------");
    }
}

uint64_t pmm_get_total_pages() {
    return total_system_pages;
}

uint64_t pmm_get_free_pages() {
    if (!pmm_buddy) return 0;
    
    // todo: use buddy_walk from the lib to get dynamic props
    return arena_total_len / 4096;
}
