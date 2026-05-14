#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <elf.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <kernel.elf> <syms.txt> <output.elf>\n", argv[0]);
        return 1;
    }
    
    fprintf(stderr, "[embsym] Opening kernel: %s\n", argv[1]);
    
    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror("fopen kernel"); return 1; }
    
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    
    fprintf(stderr, "[embsym] Kernel file size: %ld bytes\n", size);
    
    char *elf = malloc(size);
    if (!elf) { perror("malloc"); return 1; }
    
    if (fread(elf, 1, size, in) != (size_t)size) {
        perror("fread");
        return 1;
    }
    fclose(in);
    
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)elf;
    
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        return 1;
    }
    
    fprintf(stderr, "[embsym] ELF validated, %d sections\n", ehdr->e_shnum);
    
    /* Read symbol file */
    FILE *sf = fopen(argv[2], "rb");
    if (!sf) { perror("fopen syms"); return 1; }
    
    fseek(sf, 0, SEEK_END);
    long symsize = ftell(sf);
    fseek(sf, 0, SEEK_SET);
    
    fprintf(stderr, "[embsym] Symbol file size: %ld bytes\n", symsize);
    
    char *syms = malloc(symsize);
    if (!syms) { perror("malloc syms"); return 1; }
    
    if (fread(syms, 1, symsize, sf) != (size_t)symsize) {
        perror("fread syms");
        return 1;
    }
    fclose(sf);
    
    /* Find .ksyms section */
    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
        fprintf(stderr, "No section headers\n");
        return 1;
    }
    
    Elf64_Shdr *shdr = (Elf64_Shdr*)(elf + ehdr->e_shoff);
    
    if (ehdr->e_shstrndx >= ehdr->e_shnum) {
        fprintf(stderr, "Invalid shstrndx: %u >= %u\n", ehdr->e_shstrndx, ehdr->e_shnum);
        return 1;
    }
    
    Elf64_Shdr *shstr = &shdr[ehdr->e_shstrndx];
    fprintf(stderr, "[embsym] String table at offset 0x%lx, size 0x%lx\n",
            shstr->sh_offset, shstr->sh_size);
    
    if (shstr->sh_offset + shstr->sh_size > (size_t)size) {
        fprintf(stderr, "String table out of bounds\n");
        return 1;
    }
    
    char *shstrtab = elf + shstr->sh_offset;
    
    int ksyms_idx = -1;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_name >= shstr->sh_size) {
            fprintf(stderr, "[embsym] Section %d name offset %u out of bounds\n", 
                    i, shdr[i].sh_name);
            continue;
        }
        
        const char *name = shstrtab + shdr[i].sh_name;
        fprintf(stderr, "[embsym] Section %d: %s (offset 0x%lx, size 0x%lx, addr 0x%lx)\n",
                i, name, shdr[i].sh_offset, shdr[i].sh_size, shdr[i].sh_addr);
        
        if (strcmp(name, ".ksyms") == 0) {
            ksyms_idx = i;
            fprintf(stderr, "[embsym] >>> FOUND .ksyms at index %d <<<\n", i);
            break;
        }
    }
    
    if (ksyms_idx == -1) {
        fprintf(stderr, "ERROR: .ksyms section not found in ELF\n");
        return 1;
    }
    
    Elf64_Shdr *ksyms = &shdr[ksyms_idx];
    
    fprintf(stderr, "[embsym] .ksyms at file offset 0x%lx, size 0x%lx\n",
            ksyms->sh_offset, ksyms->sh_size);
    
    if (ksyms->sh_size < (uint64_t)symsize) {
        fprintf(stderr, "ERROR: .ksyms too small (%lu < %ld)\n", 
                ksyms->sh_size, symsize);
        return 1;
    }
    
    if (ksyms->sh_offset + ksyms->sh_size > (size_t)size) {
        fprintf(stderr, "ERROR: .ksyms extends past file end (offset+size=%lx > file=%ld)\n",
                ksyms->sh_offset + ksyms->sh_size, size);
        return 1;
    }
    
    /* Write symbol data into the section */
    fprintf(stderr, "[embsym] Writing %ld bytes to file offset 0x%lx\n", 
            symsize, ksyms->sh_offset);
    memcpy(elf + ksyms->sh_offset, syms, symsize);
    
    /* Verify write */
    fprintf(stderr, "[embsym] First 32 bytes after write: ");
    for (int i = 0; i < 32 && i < symsize; i++) {
        fprintf(stderr, "%02x ", (unsigned char)(elf + ksyms->sh_offset)[i]);
    }
    fprintf(stderr, "\n");
    
    /* Zero pad remainder */
    if (ksyms->sh_size > (uint64_t)symsize) {
        memset(elf + ksyms->sh_offset + symsize, 0, 
               ksyms->sh_size - symsize);
    }
    
    /* Write output */
    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror("fopen output"); return 1; }
    
    if (fwrite(elf, 1, size, out) != (size_t)size) {
        perror("fwrite");
        return 1;
    }
    fclose(out);
    
    fprintf(stderr, "[embsym] Done! Output: %s\n", argv[3]);
    
    free(elf);
    free(syms);
    return 0;
}
