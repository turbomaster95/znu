#ifndef _ELF_H
#define _ELF_H

#include <stdint.h>
#include <proc.h>

#define ELFMAG "\177ELF"

#define PT_LOAD 1

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;   // Entry point virtual address
    uint64_t e_phoff;   // Program header table offset
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;   // Number of program headers
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;  // Offset in file
    uint64_t p_vaddr;   // Virtual address in memory
    uint64_t p_paddr;
    uint64_t p_filesz;  // Size of data in file
    uint64_t p_memsz;   // Size of data in memory (can be larger for BSS)
    uint64_t p_align;
} Elf64_Phdr;

void load_elf(uint8_t* elf_data);
uint64_t* vmm_create_user_pml4(void);
process_t* create_process_from_elf(uint8_t* elf_data);

#endif
