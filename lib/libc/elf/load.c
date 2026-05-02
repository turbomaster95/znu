#include <elf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <page.h>
#include <proc.h>

extern void jump_to_usermode(uintptr_t entry, uintptr_t stack);
extern uint64_t* kernel_pml4;
extern uint64_t hhdm_offset;
extern uintptr_t vmm_virt_to_phys(uint64_t* pml4, uintptr_t virt);

uint64_t* vmm_create_user_pml4(void) {
    // 1. Allocate a new page for the PML4
    uint64_t* pml4 = (uint64_t*)PHYS_TO_VIRT(palloc_zero());

    // 2. We need the boot/kernel PML4 to clone the higher half
    // We can use the global kernel_pml4 you defined in vmm.c
    extern uint64_t* kernel_pml4;

    // 3. Clone entries 256-511 (Higher Half)
    for (int i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return pml4;
}

process_t* create_process_from_elf(uint8_t* elf_data, char** argv, char** envp) {
    static uint64_t next_pid = 1;
    // 1. Basic ELF Validation
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;
    if (memcmp(header->e_ident, ELFMAG, 4) != 0) {
        debugln("[elf] Critical: ELF has invalid magic!");
        return NULL;
    }

    // 2. Allocate process structure and private PML4
    process_t* proc = kmalloc(sizeof(process_t));
    memset(proc, 0, sizeof(process_t));
    proc->pid = next_pid++;
    proc->pml4 = vmm_create_user_pml4();
    proc->state = TASK_READY;
    proc->parent_pid = current_process ? current_process->pid : 0;
    uintptr_t max_vaddr = 0;

    // 2.5 Allocate Kernel Stack (32KB)
    void* kstack = kmalloc(32768);
    proc->kstack_top = (uintptr_t)kstack + 32768;

    if (proc->pid == 1) {
        init_process = proc;
    }

    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + header->e_phoff);

    // 3. Load PT_LOAD segments into the NEW PML4
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uintptr_t start_page = phdr[i].p_vaddr & ~0xFFFULL;
            uintptr_t end_page = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFFULL;

            // Map user pages in the new PML4
            for (uintptr_t page = start_page; page < end_page; page += 0x1000) {
                // Only map if not already present
                if (vmm_virt_to_phys(proc->pml4, page) == 0) {
                    void* phys = palloc_zero();
                    map_page(proc->pml4, page, (uintptr_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
                }
            }

            // Copy data via HHDM
            uint64_t remaining = phdr[i].p_filesz;
            uint64_t current_vaddr = phdr[i].p_vaddr;
            uint64_t src_offset = phdr[i].p_offset;

            while (remaining > 0) {
                uintptr_t phys = vmm_virt_to_phys(proc->pml4, current_vaddr);
                uint64_t page_offset = current_vaddr & 0xFFF;
                uint64_t to_copy = 0x1000 - page_offset;
                if (to_copy > remaining) to_copy = remaining;

                void* dest = (void*)(phys + hhdm_offset + page_offset);
                memcpy(dest, elf_data + src_offset, to_copy);

                current_vaddr += to_copy;
                src_offset += to_copy;
                remaining -= to_copy;
            }
            uintptr_t end_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (end_vaddr > max_vaddr) max_vaddr = end_vaddr;
        }
    }

    proc->brk_start = (max_vaddr + 0xFFF) & ~0xFFFULL;
    proc->brk = proc->brk_start;

    // 4. Setup User Stack (16 pages / 64KB)
    uintptr_t user_stack_base = 0x00007ffff0000000;
    uint64_t stack_pages = 16;
    for (uint64_t i = 0; i < stack_pages; i++) {
        void* phys = palloc_zero();
        map_page(proc->pml4, user_stack_base + (i * 0x1000), (uintptr_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    proc->entry = header->e_entry;
    uintptr_t stack_ptr = user_stack_base + (stack_pages * 0x1000);

    // Count arguments and environment
    int argc = 0;
    if (argv) while (argv[argc]) argc++;
    int envc = 0;
    if (envp) while (envp[envc]) envc++;

    // Copy environment strings
    uintptr_t* k_envp = kmalloc(sizeof(uintptr_t) * (envc + 1));
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        stack_ptr -= len;
        stack_ptr &= ~7ULL;
        void* dest = (void*)(vmm_virt_to_phys(proc->pml4, stack_ptr) + hhdm_offset);
        memcpy(dest, envp[i], len);
        k_envp[i] = stack_ptr;
    }
    k_envp[envc] = 0;

    // Copy argument strings
    uintptr_t* k_argv = kmalloc(sizeof(uintptr_t) * (argc + 1));
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        stack_ptr -= len;
        stack_ptr &= ~7ULL;
        void* dest = (void*)(vmm_virt_to_phys(proc->pml4, stack_ptr) + hhdm_offset);
        memcpy(dest, argv[i], len);
        k_argv[i] = stack_ptr;
    }
    k_argv[argc] = 0;

    // Push envp array
    for (int i = envc; i >= 0; i--) {
        stack_ptr -= 8;
        *(uint64_t*)(vmm_virt_to_phys(proc->pml4, stack_ptr) + hhdm_offset) = k_envp[i];
    }
    // Push argv array
    for (int i = argc; i >= 0; i--) {
        stack_ptr -= 8;
        *(uint64_t*)(vmm_virt_to_phys(proc->pml4, stack_ptr) + hhdm_offset) = k_argv[i];
    }

    // Push argc
    stack_ptr -= 8;
    *(uint64_t*)(vmm_virt_to_phys(proc->pml4, stack_ptr) + hhdm_offset) = (uint64_t)argc;

    kfree(k_envp);
    kfree(k_argv);

    proc->stack_top = stack_ptr;

    // Initialize context for the first run on the new kernel stack
    registers_t* regs = (registers_t*)(proc->kstack_top - sizeof(registers_t));
    memset(regs, 0, sizeof(registers_t));
    
    regs->rip = proc->entry;
    regs->rsp = proc->stack_top;
    regs->cs = 0x23;
    regs->ss = 0x1B;
    regs->ds = 0x1B;
    regs->es = 0x1B;
    regs->rflags = 0x202; // IF = 1
    
    proc->context_ptr = regs;
    
    // Initialize SSE state
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : "=m"(proc->sse_state));
    // Set default MXCSR (bits 7-12 are masks, they should be 1 to mask exceptions)
    *(uint32_t*)&proc->sse_state[24] = 0x1F80; 

    debugln("[proc] Init process created. Entry: %p, PML4: %p", proc->entry, proc->pml4);
    
    // Explicitly reserve FD 0, 1, 2 for console/keyboard
    for (int i = 0; i < 16; i++) proc->files[i] = NULL;
    proc->files[0] = (void*)0x1; // stdin marker
    proc->files[1] = (void*)0x1; // stdout marker
    proc->files[2] = (void*)0x1; // stderr marker

    return proc;
}

void load_elf(uint8_t* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;

    if (memcmp(header->e_ident, ELFMAG, 4) != 0) {
        debugln("[elf] Critical: Init ELF has invalid magic!");
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
		debugln("[elf] Remaining abt to be memcpy'd. dest: %p, elf + src_offset: %p, to_copy: %p", dest, elf_data + src_offset, to_copy);
                memcpy(dest, elf_data + src_offset, to_copy);

                current_vaddr += to_copy;
                src_offset += to_copy;
                remaining -= to_copy;
		debugln("[elf] memcpy'd done!");
            }
        }
    }

    uintptr_t user_stack_base = 0x00007ffff0000000;
    uint64_t stack_pages = 16; // 64 KiB

    for (uint64_t i = 0; i < stack_pages; i++) {
        void* phys = palloc_zero();
	debugln("[elf] Map page phys: %p, ustackbase: %p, kernel_pm4: %p", (uintptr_t)phys, user_stack_base + (i * 0x1000), kernel_pml4);
        map_page(kernel_pml4, user_stack_base + (i * 0x1000), (uintptr_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    uintptr_t user_stack_top = user_stack_base + (stack_pages * 0x1000) - 16;
    debugln("[elf] About to jmp to userspace, hdr=> %lx, ustacktop: %p", header->e_entry, user_stack_top);
    jump_to_usermode(header->e_entry, user_stack_top);
}

