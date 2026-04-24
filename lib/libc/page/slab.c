#include <page.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

// A dedicated virtual address range for the kernel heap
static uint64_t heap_virtual_top;

struct slab_header {
    struct slab_header* next_slab;
    void* free_list;
    uint32_t slot_size;
    uint32_t slots_free;
};

static struct slab_header* buckets[8];
static const uint32_t bucket_sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};

/**
 * init_slab: Sets up the starting virtual address for the heap.
 * Call this in kmain after init_vmm.
 */
void init_slab(void) {
    debugln("[SLAB] Initializing Slab Allocator...");
    // Starting heap at a safe high-half address
    heap_virtual_top = 0xffff900000000000; 
    
    for (int i = 0; i < 8; i++) {
        buckets[i] = NULL;
    }
    debugln("[SLAB] Heap start: %p", heap_virtual_top);
}

static int get_bucket_idx(uint64_t size) {
    for (int i = 0; i < 8; i++) {
        if (size <= bucket_sizes[i]) return i;
    }
    return -1;
}

static void* heap_extend(uint64_t pages) {
    void* virt_addr = (void*)heap_virtual_top;

    for (uint64_t i = 0; i < pages; i++) {
        void* phys = palloc_zero();
        if (!phys) {
            debugln("[SLAB] CRITICAL: Out of physical memory during heap extension!");
            return NULL;
        }

        // Map the new physical page into our virtual heap range
        map_page(vmm_get_kernel_pml4(), heap_virtual_top, (uint64_t)phys, PTE_WRITABLE);
        heap_virtual_top += PAGE_SIZE;
    }

    return virt_addr;
}

void* kmalloc(uint64_t size) {
    if (size == 0) return NULL;

    int idx = get_bucket_idx(size);

    // Large allocation (Big Alloc)
    if (idx == -1) {
        uint64_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        void* ptr = heap_extend(pages_needed);
        return ptr;
    }

    struct slab_header* slab = buckets[idx];

    // Allocate new slab if bucket is empty or current slab is full
    if (!slab || slab->slots_free == 0) {
        void* new_page_virt = heap_extend(1);
        if (!new_page_virt) return NULL;

        struct slab_header* new_slab = (struct slab_header*)new_page_virt;
        new_slab->slot_size = bucket_sizes[idx];
        new_slab->slots_free = (PAGE_SIZE - sizeof(struct slab_header)) / new_slab->slot_size;
        new_slab->next_slab = buckets[idx];

        uint8_t* first_slot = (uint8_t*)new_page_virt + sizeof(struct slab_header);
        new_slab->free_list = first_slot;

        for (uint32_t i = 0; i < new_slab->slots_free - 1; i++) {
            void** current = (void**)(first_slot + (i * new_slab->slot_size));
            *current = (void*)(first_slot + ((i + 1) * new_slab->slot_size));
        }

        void** last = (void**)(first_slot + ((new_slab->slots_free - 1) * new_slab->slot_size));
        *last = NULL;

        buckets[idx] = new_slab;
        slab = new_slab;
    }

    void* addr = slab->free_list;
    slab->free_list = *(void**)addr;
    slab->slots_free--;

    return addr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    // A slab header always sits at the start of a 4KB aligned page
    struct slab_header* slab = (struct slab_header*)((uint64_t)ptr & ~0xFFF);

    if (slab->slot_size == 0) return; // Not a slab allocation (Big Alloc)

    *(void**)ptr = slab->free_list;
    slab->free_list = ptr;
    slab->slots_free++;
}

