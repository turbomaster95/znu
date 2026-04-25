#include <elf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <page.h>

extern void jump_to_usermode(uintptr_t entry, uintptr_t stack);
extern uint64_t* kernel_pml4;
void load_elf(uint8_t* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;

    // 1. Validation
    if (header->e_ident[0] != ELFMAG0 || header->e_ident[1] != ELFMAG1) {
        debugln("[ELF] Invalid Magic!");
        return;
    }

    // 2. Iterate Program Headers
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            debugln("[ELF] Loading segment at %p, size: %d", phdr[i].p_vaddr, phdr[i].p_memsz);

            // Calculate how many pages we need
            uint64_t pages = (phdr[i].p_memsz + 0xFFF) / 0x1000;
            
            // 3. Allocate and Map for User
            for (uint64_t j = 0; j < pages; j++) {
                uintptr_t virt = phdr[i].p_vaddr + (j * 0x1000);
                void* phys = palloc_zero(); // Allocate a fresh physical frame
                
                // Use your fixed map_page that propagates PTE_USER!
                map_page(kernel_pml4, virt, (uintptr_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
            }

            // 4. Copy data from the file into the newly mapped virtual memory
            // Note: Since we are in the kernel, we can access phdr[i].p_vaddr 
            // if we are using the same PML4.
            memcpy((void*)phdr[i].p_vaddr, elf_data + phdr[i].p_offset, phdr[i].p_filesz);

            // 5. Zero out the BSS (if memsz > filesz)
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((void*)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    // 6. Set up User Stack and Jump
    uint8_t* user_stack = (uint8_t*)kmalloc(16384); 
    uintptr_t user_stack_top = (uintptr_t)user_stack + 16384;
    
    // Map the stack as user too!
    for(int i=0; i<4; i++) { // 16KB stack
        uintptr_t s_virt = (uintptr_t)user_stack + (i * 0x1000);
        map_page(kernel_pml4, s_virt, vmm_virt_to_phys(kernel_pml4, s_virt), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    debugln("[ELF] Entry Point: %p. Jumping...", header->e_entry);
    jump_to_usermode(header->e_entry, user_stack_top);
}

