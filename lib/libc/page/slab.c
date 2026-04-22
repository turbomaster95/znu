#include <page.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

// Each slab manages a specific power-of-two size
struct slab_header {
    struct slab_header* next_slab; // Next page of same slot size
    void* free_list;              // Pointer to first free slot in this page
    uint32_t slot_size;
    uint32_t slots_free;
};

// We define buckets for common sizes
static struct slab_header* buckets[8]; // 16, 32, 64, 128, 256, 512, 1024, 2048
static const uint32_t bucket_sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};

// Helper: Find which bucket fits the requested size
static int get_bucket_idx(uint64_t size) {
    for (int i = 0; i < 8; i++) {
        if (size <= bucket_sizes[i]) return i;
    }
    return -1; // Too big for slab, use palloc directly
}

void* kmalloc(uint64_t size) {
    int idx = get_bucket_idx(size);
    if (idx == -1) return palloc_zero(); // Fallback for large allocations

    struct slab_header* slab = buckets[idx];

    // Find a slab with free space, or create a new one
    if (!slab || slab->slots_free == 0) {
        // Request a new page from your existing PMM
        void* new_page_phys = palloc_zero(); 
        void* new_page_virt = (void*)((uint64_t)new_page_phys + hhdm_offset);

        struct slab_header* new_slab = (struct slab_header*)new_page_virt;
        new_slab->slot_size = bucket_sizes[idx];
        new_slab->slots_free = (4096 - sizeof(struct slab_header)) / new_slab->slot_size;
        new_slab->next_slab = buckets[idx];
        
        // Build the free list inside the page
        uint8_t* first_slot = (uint8_t*)new_page_virt + sizeof(struct slab_header);
        new_slab->free_list = first_slot;

        // Chain the slots: each slot stores the address of the next
        for (uint32_t i = 0; i < new_slab->slots_free - 1; i++) {
            void** current = (void**)(first_slot + (i * new_slab->slot_size));
            *current = (void*)(first_slot + ((i + 1) * new_slab->slot_size));
        }
        
        // Last slot points to NULL
        void** last = (void**)(first_slot + ((new_slab->slots_free - 1) * new_slab->slot_size));
        *last = NULL;

        buckets[idx] = new_slab;
        slab = new_slab;
    }

    // Pop a slot off the free list
    void* addr = slab->free_list;
    slab->free_list = *(void**)addr;
    slab->slots_free--;
    debugln("Allocated: %p", addr);

    return addr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    // To find the slab header, we align the pointer to 4KB (page start)
    struct slab_header* slab = (struct slab_header*)((uint64_t)ptr & ~0xFFF);
    
    // Check if this was a PMM allocation or a slab allocation
    // (In a full kernel, you'd check if ptr is in a slab range)
    
    // Push back onto the free list
    *(void**)ptr = slab->free_list;
    slab->free_list = ptr;
    slab->slots_free++;
}

