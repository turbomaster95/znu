#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <stdio.h>
#include <symbols.h>

#define MAX_MODULES 64
#define MOD_NAME_LEN 64

typedef struct module {
    char        name[MOD_NAME_LEN];
    void       *base;           /* kmalloc'd block */
    size_t      size;
    void       *init_fn;
    void       *exit_fn;
    bool        loaded;
    Elf64_Ehdr *ehdr;           /* points into base */
    Elf64_Sym  *symtab;         /* module's own symbol table */
    const char *strtab;         /* module's string table */
    uint32_t    sym_count;
} module_t;

static module_t modules[MAX_MODULES];
static uint32_t mod_count = 0;

static int elf_validate_reloc(const Elf64_Ehdr *hdr)
{
    if (memcmp(hdr->e_ident, ELFMAG, 4) != 0) {
        debugln("[mod] Bad magic");
        return -1;
    }
    if (hdr->e_ident[EI_CLASS]   != ELFCLASS64) {
        debugln("[mod] Not ELF64");
        return -1;
    }
    if (hdr->e_ident[EI_DATA]    != ELFDATA2LSB) {
        debugln("[mod] Not little-endian");
        return -1;
    }
    if (hdr->e_machine           != EM_X86_64) {
        debugln("[mod] Not x86-64");
        return -1;
    }
    if (hdr->e_type != ET_REL) {
        debugln("[mod] Not relocatable (ET_REL)");
        return -1;
    }
    return 0;
}

/* Get section header string table */
static const char *elf_shstrtab(const Elf64_Ehdr *hdr)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
    if (hdr->e_shstrndx == SHN_UNDEF) return NULL;
    return (const char *)((uintptr_t)hdr + shdr[hdr->e_shstrndx].sh_offset);
}

static Elf64_Shdr *elf_sheader(const Elf64_Ehdr *hdr, uint16_t idx)
{
    if (idx >= hdr->e_shnum) return NULL;
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
    return &shdr[idx];
}

static const char *elf_section_name(const Elf64_Ehdr *hdr, Elf64_Shdr *sh)
{
    const char *strtab = elf_shstrtab(hdr);
    if (!strtab || !sh) return "";
    return strtab + sh->sh_name;
}

/* Calculate total size needed for ALLOC sections and build a layout map */
static size_t calc_alloc_size(const Elf64_Ehdr *hdr, uint16_t **out_idx,
                               size_t **out_off, size_t *out_nalloc)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
    size_t nalloc = 0;

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_flags & SHF_ALLOC) nalloc++;
    }

    uint16_t *idx = kmalloc(sizeof(uint16_t) * nalloc);
    size_t   *off = kmalloc(sizeof(size_t)   * nalloc);
    if (!idx || !off) { kfree(idx); kfree(off); return 0; }

    size_t total = 0;
    size_t j = 0;
    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (!(shdr[i].sh_flags & SHF_ALLOC)) continue;

        /* Align up to section alignment */
        size_t align = shdr[i].sh_addralign;
        if (align > 1) total = (total + align - 1) & ~(align - 1);

        idx[j] = i;
        off[j] = total;
        total += shdr[i].sh_size;
        j++;
    }

    *out_idx = idx;
    *out_off = off;
    *out_nalloc = nalloc;
    return total;
}


static Elf64_Sym *find_module_sym(module_t *mod, const char *name)
{
    if (!mod->symtab || !mod->strtab) return NULL;
    for (uint32_t i = 0; i < mod->sym_count; i++) {
        const char *s = mod->strtab + mod->symtab[i].st_name;
        if (strcmp(s, name) == 0) return &mod->symtab[i];
    }
    return NULL;
}

/* Resolve a symbol: first module's own table, then kernel symbol table */
static uint64_t resolve_symbol(module_t *mod, const char *name)
{
    Elf64_Sym *ms = find_module_sym(mod, name);
    if (ms && ms->st_shndx != SHN_UNDEF)
        return (uint64_t)mod->base + ms->st_value;

    uint64_t kaddr = sym_get_addr(name);
    if (kaddr) return kaddr;

    debugln("[mod] Unresolved symbol: %s", name);
    return 0;
}

static int apply_rela(module_t *mod, Elf64_Rela *rela, uint64_t symval,
                       uint16_t shndx, size_t sec_offset)
{
    uint64_t *where = (uint64_t *)((uintptr_t)mod->base + sec_offset + rela->r_offset);
    uint64_t S = symval;                    /* symbol value */
    uint64_t A = rela->r_addend;            /* addend */
    uint64_t P = (uint64_t)where;           /* place of relocation */
    uint64_t B = (uint64_t)mod->base;      /* base of module */

    switch (ELF64_R_TYPE(rela->r_info)) {

    case R_X86_64_64:       /* S + A */
        *where = S + A;
        break;

    case R_X86_64_PC32:     /* S + A - P */
    case R_X86_64_PLT32:    /* S + A - P (treat same for kernel modules) */
        *(uint32_t *)where = (uint32_t)(S + A - P);
        break;

    case R_X86_64_32:       /* S + A */
        *(uint32_t *)where = (uint32_t)(S + A);
        break;

    case R_X86_64_32S:      /* S + A (signed 32-bit) */
        *(int32_t *)where = (int32_t)(S + A);
        break;

    case R_X86_64_GOTPCREL: /* G + GOT + A - P  (simplified: no GOT) */
    case R_X86_64_REX_GOTPCRELX:
        debugln("[mod] GOTPCREL relocation not supported");
        return -1;

    default:
        debugln("[mod] Unknown relocation type %lu",
                ELF64_R_TYPE(rela->r_info));
        return -1;
    }
    return 0;
}

static int process_relocations(module_t *mod, const Elf64_Ehdr *hdr,
                                uint16_t *sec_idx, size_t *sec_off, size_t nalloc)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type != SHT_RELA) continue;

        /* Target section being relocated */
        uint16_t target = shdr[i].sh_info;
        if (target >= hdr->e_shnum) continue;
        if (!(shdr[target].sh_flags & SHF_ALLOC)) continue;

        /* Symbol table for this relocation section */
        uint16_t symtab_idx = shdr[i].sh_link;
        if (symtab_idx >= hdr->e_shnum) continue;
        Elf64_Shdr *symtab_hdr = &shdr[symtab_idx];
        Elf64_Sym  *symtab = (Elf64_Sym *)((uintptr_t)hdr + symtab_hdr->sh_offset);
        const char *strtab = (const char *)((uintptr_t)hdr +
                                             shdr[symtab_hdr->sh_link].sh_offset);

        Elf64_Rela *rela = (Elf64_Rela *)((uintptr_t)hdr + shdr[i].sh_offset);
        size_t nrela = shdr[i].sh_size / sizeof(Elf64_Rela);

        /* Find target section's offset in our allocated block */
        size_t target_off = 0;
        bool found = false;
        for (size_t j = 0; j < nalloc; j++) {
            if (sec_idx[j] == target) { target_off = sec_off[j]; found = true; break; }
        }
        if (!found) continue;

        for (size_t r = 0; r < nrela; r++) {
            uint32_t symidx = ELF64_R_SYM(rela[r].r_info);
            if (symidx >= symtab_hdr->sh_size / sizeof(Elf64_Sym)) continue;

            Elf64_Sym *sym = &symtab[symidx];
            const char *symname = strtab + sym->st_name;
            uint64_t symval = 0;

            if (sym->st_shndx == SHN_UNDEF) {
                /* External symbol — resolve via kernel or other modules */
                symval = resolve_symbol(mod, symname);
                if (!symval) {
                    debugln("[mod] Cannot resolve '%s'", symname);
                    return -1;
                }
            } else if (sym->st_shndx == SHN_ABS) {
                symval = sym->st_value;
            } else if (sym->st_shndx < hdr->e_shnum) {
                /* Section-relative symbol */
                size_t sec_off_local = 0;
                for (size_t j = 0; j < nalloc; j++) {
                    if (sec_idx[j] == sym->st_shndx) {
                        sec_off_local = sec_off[j];
                        break;
                    }
                }
                symval = (uint64_t)mod->base + sec_off_local + sym->st_value;
            } else {
                debugln("[mod] Bad symbol section %u", sym->st_shndx);
                return -1;
            }

            if (apply_rela(mod, &rela[r], symval, target, target_off) < 0)
                return -1;
        }
    }
    return 0;
}

static void find_module_hooks(module_t *mod, const Elf64_Ehdr *hdr)
{
    if (!mod->symtab || !mod->strtab) return;

    for (uint32_t i = 0; i < mod->sym_count; i++) {
        Elf64_Sym *s = &mod->symtab[i];
        if (s->st_shndx == SHN_UNDEF) continue;

        const char *name = mod->strtab + s->st_name;
        uint64_t addr = (uint64_t)mod->base + s->st_value;

        if (strcmp(name, "module_init") == 0) mod->init_fn = (void *)addr;
        else if (strcmp(name, "module_exit") == 0) mod->exit_fn = (void *)addr;
    }
}

module_t *load_kernel_module(const char *name, uint8_t *elf_data, size_t len)
{
    if (!elf_data || len < sizeof(Elf64_Ehdr)) return NULL;
    if (mod_count >= MAX_MODULES) {
        debugln("[mod] Module table full");
        return NULL;
    }

    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;
    if (elf_validate_reloc(hdr) < 0) return NULL;

    /* Ensure symbols are parsed */
    symbols_init();

    /* Calculate layout for ALLOC sections */
    uint16_t *sec_idx = NULL;
    size_t   *sec_off = NULL;
    size_t    nalloc = 0;
    size_t    total_size = calc_alloc_size(hdr, &sec_idx, &sec_off, &nalloc);
    if (total_size == 0 || !sec_idx || !sec_off) {
        kfree(sec_idx); kfree(sec_off);
        debugln("[mod] No allocatable sections");
        return NULL;
    }

    /* Allocate module block */
    void *base = kmalloc(total_size);
    if (!base) {
        kfree(sec_idx); kfree(sec_off);
        debugln("[mod] kmalloc failed for %lu bytes", total_size);
        return NULL;
    }
    memset(base, 0, total_size);

    /* Copy sections into allocated block */
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
    for (size_t j = 0; j < nalloc; j++) {
        uint16_t i = sec_idx[j];
        void *dest = (void *)((uintptr_t)base + sec_off[j]);
        if (shdr[i].sh_type != SHT_NOBITS && shdr[i].sh_size > 0) {
            memcpy(dest, elf_data + shdr[i].sh_offset, shdr[i].sh_size);
        }
    }

    /* Set up module structure */
    module_t *mod = &modules[mod_count++];
    memset(mod, 0, sizeof(*mod));
    mod->base     = base;
    mod->size     = total_size;
    mod->ehdr     = (Elf64_Ehdr *)base;
    mod->loaded   = true;

    /* Copy name safely */
    size_t nlen = strlen(name);
    if (nlen >= MOD_NAME_LEN) nlen = MOD_NAME_LEN - 1;
    memcpy(mod->name, name, nlen);
    mod->name[nlen] = '\0';

    /* Find module's own symbol table (now loaded in memory) */
    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            /* Symbol table was copied; find where it landed */
            for (size_t j = 0; j < nalloc; j++) {
                if (sec_idx[j] == i) {
                    mod->symtab = (Elf64_Sym *)((uintptr_t)base + sec_off[j]);
                    mod->sym_count = shdr[i].sh_size / sizeof(Elf64_Sym);
                    /* String table */
                    uint16_t stridx = shdr[i].sh_link;
                    for (size_t k = 0; k < nalloc; k++) {
                        if (sec_idx[k] == stridx) {
                            mod->strtab = (const char *)((uintptr_t)base + sec_off[k]);
                            break;
                        }
                    }
                    break;
                }
            }
            break; /* Only one SHT_SYMTAB expected */
        }
    }

    /* Process relocations */
    if (process_relocations(mod, hdr, sec_idx, sec_off, nalloc) < 0) {
        debugln("[mod] Relocation failed");
        kfree(base);
        mod->loaded = false;
        mod_count--;
        kfree(sec_idx); kfree(sec_off);
        return NULL;
    }

    kfree(sec_idx);
    kfree(sec_off);

    /* Find init/exit hooks */
    find_module_hooks(mod, hdr);

    debugln("[mod] Loaded '%s' at %p, size %lu", mod->name, mod->base, mod->size);

    /* Call init if present */
    if (mod->init_fn) {
        debugln("[mod] Calling %s::module_init", mod->name);
        int ret = ((int (*)(void))mod->init_fn)();
        if (ret != 0) {
            debugln("[mod] %s::module_init returned %d", mod->name, ret);
        }
    }

    return mod;
}

int unload_kernel_module(module_t *mod)
{
    if (!mod || !mod->loaded) return -1;

    if (mod->exit_fn) {
        debugln("[mod] Calling %s::module_exit", mod->name);
        ((void (*)(void))mod->exit_fn)();
    }

    
/* TODO: check refcounts, deregister from subsystems, etc. */

    if (mod->base) kfree(mod->base);

    mod->loaded = false;
    mod->base = NULL;
    mod->init_fn = NULL;
    mod->exit_fn = NULL;

    debugln("[mod] Unloaded '%s'", mod->name);
    return 0;
}

module_t *mod_find(const char *name)
{
    for (uint32_t i = 0; i < mod_count; i++) {
        if (modules[i].loaded && strcmp(modules[i].name, name) == 0)
            return &modules[i];
    }
    return NULL;
}

void mod_list(void)
{
    debugln("[mod] Loaded modules:");
    for (uint32_t i = 0; i < mod_count; i++) {
        if (!modules[i].loaded) continue;
        debugln("  [%u] %s @ %p (size %lu, init=%p, exit=%p)",
                i, modules[i].name, modules[i].base, modules[i].size,
                modules[i].init_fn, modules[i].exit_fn);
    }
}
