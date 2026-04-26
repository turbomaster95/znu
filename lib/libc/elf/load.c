#include <elf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <page.h>

extern void jump_to_usermode(uintptr_t entry, uintptr_t stack);
extern uint64_t* kernel_pml4;
extern uint64_t hhdm_offset;
extern uintptr_t vmm_virt_to_phys(uint64_t* pml4, uintptr_t virt);

void load_elf(uint8_t* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;

    if (header->e_ident[0] != ELFMAG0 || header->e_ident[1] != ELFMAG1 ||
        header->e_ident[2] != ELFMAG2 || header->e_ident[3] != ELFMAG3) {
        debugln("[ELF] Invalid Magic!");
        return;
    }

    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + header->e_phoff);
    debugln("[elf] Got PHDR's");

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uintptr_t start_page = phdr[i].p_vaddr & ~0xFFFULL;
            uintptr_t end_page = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFFULL;

            // 1. Map the pages for the segment
            for (uintptr_t page = start_page; page < end_page; page += 0x1000) {
                if (vmm_virt_to_phys(kernel_pml4, page) == 0) {
                    void* phys = palloc_zero();
		    debugln("Mapping page phys: %p, page: %p, kernel_pm4: %p", (uintptr_t)phys, page, kernel_pml4);
                    map_page(kernel_pml4, page, (uintptr_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
                }
            }

            debugln("[elf] Pages Mapped");
            // 2. Copy data via HHDM to bypass SMAP
            // Note: BSS is automatically zeroed because palloc_zero provides clean pages.
            uint64_t remaining = phdr[i].p_filesz;
            uint64_t current_vaddr = phdr[i].p_vaddr;
            uint64_t src_offset = phdr[i].p_offset;

            while (remaining > 0) {
                uintptr_t phys = vmm_virt_to_phys(kernel_pml4, current_vaddr);
                uint64_t page_offset = current_vaddr & 0xFFF;
                uint64_t to_copy = 0x1000 - page_offset;
                if (to_copy > remaining) to_copy = remaining;

                // Write directly to physical memory via the higher-half map
                void* dest = (void*)(phys + hhdm_offset);
		debugln("Remaining abt to be memcpy'd. dest: %p, elf + src_offset: %p, to_copy: %p", dest, elf_data + src_offset, to_copy);
                memcpy(dest, elf_data + src_offset, to_copy);

                current_vaddr += to_copy;
                src_offset += to_copy;
                remaining -= to_copy;
		debugln("[elf] memcpy'd done!");
            }
        }
    }

    uintptr_t user_stack_base = 0x00007ffff0000000;
    uint64_t stack_pages = 4; // 16 KiB

    for (uint64_t i = 0; i < stack_pages; i++) {
        void* phys = palloc_zero();
	debugln("Map page phys: %p, ustackbase: %p, kernel_pm4: %p", (uintptr_t)phys, user_stack_base + (i * 0x1000), kernel_pml4);
        map_page(kernel_pml4, user_stack_base + (i * 0x1000), (uintptr_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    uintptr_t user_stack_top = user_stack_base + (stack_pages * 0x1000);
    debugln("[elf] About to jmp to userspace, hdr=> %s, ustacktop: %p", header->e_entry, user_stack_top);
    jump_to_usermode(header->e_entry, user_stack_top);
}

