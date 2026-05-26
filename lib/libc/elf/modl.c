#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <stdio.h>
#include <symbols.h>
#include <errno.h>

#define MAX_MODULES 64
#define MOD_NAME_LEN 64

typedef struct module {
    char        name[MOD_NAME_LEN];
    void       *base;
    size_t      size;
    void       *init_fn;
    void       *exit_fn;
    bool        loaded;
    Elf64_Sym  *symtab;         
    const char *strtab;         
    uint32_t    sym_count;
} module_t;

static module_t modules[MAX_MODULES];
static uint32_t mod_count = 0;

static int elf_validate_reloc(const Elf64_Ehdr *hdr)
{
    if (memcmp(hdr->e_ident, ELFMAG, 4) != 0) { debugln("[mod] Bad magic!"); return -ENOEXEC; }
    if (hdr->e_ident[EI_CLASS]   != ELFCLASS64) { debugln("[mod] Not ELF64!"); return -ENOEXEC; }
    if (hdr->e_ident[EI_DATA]    != ELFDATA2LSB) { debugln("[mod] Not Little Endian!"); return -ENOEXEC; }
    if (hdr->e_machine           != EM_X86_64) { debugln("[mod] Not x86-64!"); return -ENOEXEC; }
    if (hdr->e_type != ET_REL) { debugln("[mod] Not ET_REL!"); return -ENOEXEC; }
    return 0;
}

static size_t calc_alloc_size(const Elf64_Ehdr *hdr, uint16_t **out_idx,
                               size_t **out_off, size_t *out_nalloc)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
    size_t nalloc = 0;
    for (uint16_t i = 0; i < hdr->e_shnum; i++)
        if (shdr[i].sh_flags & SHF_ALLOC) nalloc++;

    if (nalloc == 0) { *out_nalloc = 0; return 0; }

    uint16_t *idx = kmalloc(sizeof(uint16_t) * nalloc);
    size_t   *off = kmalloc(sizeof(size_t)   * nalloc);
    if (!idx || !off) { kfree(idx); kfree(off); return 0; }

    size_t total = 0, j = 0;
    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (!(shdr[i].sh_flags & SHF_ALLOC)) continue;
        size_t align = shdr[i].sh_addralign;
        if (align < 1) align = 1;
        total = (total + align - 1) & ~(align - 1);
        idx[j] = i;
        off[j] = total;
        total += shdr[i].sh_size;
        j++;
    }
    *out_idx = idx; *out_off = off; *out_nalloc = nalloc;
    return total;
}

static uint64_t resolve_symbol(module_t *mod, const char *name)
{
    uint64_t kaddr = sym_get_addr(name);
    if (kaddr) return kaddr;
    debugln("[mod] Unresolved: %s", name);
    return 0;
}

static int apply_rela(module_t *mod, Elf64_Rela *rela, uint64_t symval, size_t sec_offset)
{
    uint64_t *where = (uint64_t *)((uintptr_t)mod->base + sec_offset + rela->r_offset);
    uint64_t S = symval, A = rela->r_addend, P = (uint64_t)where;

    switch (ELF64_R_TYPE(rela->r_info)) {
    case R_X86_64_64:    *where = S + A; break;
    case R_X86_64_PC32:
    case R_X86_64_PLT32: *(uint32_t *)where = (uint32_t)(S + A - P); break;
    case R_X86_64_32:    *(uint32_t *)where = (uint32_t)(S + A); break;
    case R_X86_64_32S:   *(int32_t *)where  = (int32_t)(S + A); break;
    default:
        debugln("[mod] Unknown reloc %lu", ELF64_R_TYPE(rela->r_info));
        return -ENOSYS;
    }
    return 0;
}

static int process_relocations(module_t *mod, const Elf64_Ehdr *hdr,
                                uint16_t *sec_idx, size_t *sec_off, size_t nalloc)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type != SHT_RELA) continue;
        uint16_t target = shdr[i].sh_info;
        if (target >= hdr->e_shnum || !(shdr[target].sh_flags & SHF_ALLOC)) continue;

        uint16_t symtab_idx = shdr[i].sh_link;
        if (symtab_idx >= hdr->e_shnum) continue;
        Elf64_Shdr *symtab_hdr = &shdr[symtab_idx];
        Elf64_Sym  *symtab = (Elf64_Sym *)((uintptr_t)hdr + symtab_hdr->sh_offset);
        const char *strtab = (const char *)((uintptr_t)hdr + shdr[symtab_hdr->sh_link].sh_offset);

        Elf64_Rela *rela = (Elf64_Rela *)((uintptr_t)hdr + shdr[i].sh_offset);
        size_t nrela = shdr[i].sh_size / sizeof(Elf64_Rela);

        size_t target_off = 0;
        for (size_t j = 0; j < nalloc; j++)
            if (sec_idx[j] == target) { target_off = sec_off[j]; break; }

        for (size_t r = 0; r < nrela; r++) {
            uint32_t symidx = ELF64_R_SYM(rela[r].r_info);
            Elf64_Sym *sym = &symtab[symidx];
            const char *symname = strtab + sym->st_name;
            uint64_t symval = 0;

            if (sym->st_shndx == SHN_UNDEF) {
                symval = resolve_symbol(mod, symname);
                if (!symval) return -ENOENT;
            } else if (sym->st_shndx == SHN_ABS) {
                symval = sym->st_value;
            } else if (sym->st_shndx < hdr->e_shnum) {
                size_t soff = 0;
                for (size_t j = 0; j < nalloc; j++)
                    if (sec_idx[j] == sym->st_shndx) { soff = sec_off[j]; break; }
                symval = (uint64_t)mod->base + soff + sym->st_value;
            } else {
                return -EINVAL;
            }

            if (apply_rela(mod, &rela[r], symval, target_off) < 0) return -1;
        }
    }
    return 0;
}

static void find_module_hooks(module_t *mod, const Elf64_Ehdr *hdr,
                               uint16_t *sec_idx, size_t *sec_off, size_t nalloc)
{
    if (!mod->symtab || !mod->strtab) {
        debugln("[mod] No symtab/strtab for hook search");
        return;
    }

    for (uint32_t i = 0; i < mod->sym_count; i++) {
        Elf64_Sym *s = &mod->symtab[i];
        if (s->st_shndx == SHN_UNDEF || s->st_shndx == SHN_ABS) continue;

        size_t sec_base = 0;
        bool found = false;
        for (size_t j = 0; j < nalloc; j++) {
            if (sec_idx[j] == s->st_shndx) {
                sec_base = sec_off[j];
                found = true;
                break;
            }
        }
        if (!found) continue;

        const char *name = mod->strtab + s->st_name;
        uint64_t addr = (uint64_t)mod->base + sec_base + s->st_value;

        if (strcmp(name, "module_init") == 0) {
            mod->init_fn = (void *)addr;
            debugln("[mod] module_init @ %p", (void*)addr);
        } else if (strcmp(name, "module_exit") == 0) {
            mod->exit_fn = (void *)addr;
            debugln("[mod] module_exit @ %p", (void*)addr);
        }
    }
}

module_t *load_kernel_module(const char *name, uint8_t *elf_data, size_t len)
{
    if (!elf_data || len < sizeof(Elf64_Ehdr)) return NULL;
    if (mod_count >= MAX_MODULES) { debugln("[mod] Full"); return NULL; }

    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;
    if (elf_validate_reloc(hdr) < 0) return NULL;

    symbols_init();

    uint16_t *sec_idx = NULL;
    size_t   *sec_off = NULL;
    size_t    nalloc = 0;
    size_t    total = calc_alloc_size(hdr, &sec_idx, &sec_off, &nalloc);
    if (!total || !sec_idx) { kfree(sec_idx); kfree(sec_off); return NULL; }

    void *base = kmalloc(total);
    if (!base) { kfree(sec_idx); kfree(sec_off); return NULL; }
    memset(base, 0, total);

    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
    for (size_t j = 0; j < nalloc; j++) {
        uint16_t i = sec_idx[j];
        void *dest = (void *)((uintptr_t)base + sec_off[j]);
        if (shdr[i].sh_type != SHT_NOBITS && shdr[i].sh_size > 0)
            memcpy(dest, elf_data + shdr[i].sh_offset, shdr[i].sh_size);
    }

    module_t *mod = &modules[mod_count++];
    memset(mod, 0, sizeof(*mod));
    mod->base = base; mod->size = total; mod->loaded = true;
    size_t nlen = strlen(name);
    if (nlen >= MOD_NAME_LEN) nlen = MOD_NAME_LEN - 1;
    memcpy(mod->name, name, nlen); mod->name[nlen] = '\0';

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            mod->symtab = (Elf64_Sym *)(elf_data + shdr[i].sh_offset);
            mod->sym_count = shdr[i].sh_size / sizeof(Elf64_Sym);
            uint16_t stridx = shdr[i].sh_link;
            if (stridx < hdr->e_shnum) {
                mod->strtab = (const char *)(elf_data + shdr[stridx].sh_offset);
            }
            debugln("[mod] symtab=%p count=%u strtab=%p",
                    mod->symtab, mod->sym_count, mod->strtab);
            break;
        }
    }

    if (process_relocations(mod, hdr, sec_idx, sec_off, nalloc) < 0) {
        debugln("[mod] Reloc failed");
        kfree(base); mod->loaded = false; mod_count--;
        kfree(sec_idx); kfree(sec_off);
        return NULL;
    }

    find_module_hooks(mod, hdr, sec_idx, sec_off, nalloc);

    kfree(sec_idx); kfree(sec_off);

    debugln("[mod] Loaded '%s' base=%p size=%lu init=%p exit=%p",
            mod->name, mod->base, mod->size, mod->init_fn, mod->exit_fn);

    if (mod->init_fn) {
        debugln("[mod] Calling %s::module_init", mod->name);
        int ret = ((int (*)(void))mod->init_fn)();
        debugln("[mod] init returned %d", ret);
    } else {
        debugln("[mod] WARNING: no module_init!");
    }

    return mod;
}

int unload_kernel_module(module_t *mod)
{
    if (!mod || !mod->loaded) return -EINVAL;
    if (mod->exit_fn) { debugln("[mod] %s::module_exit", mod->name); ((void (*)(void))mod->exit_fn)(); }
    if (mod->base) kfree(mod->base);
    mod->loaded = false;
    return 0;
}

module_t *mod_find(const char *name)
{
    for (uint32_t i = 0; i < mod_count; i++)
        if (modules[i].loaded && strcmp(modules[i].name, name) == 0) return &modules[i];
    return NULL;
}

void mod_list(void)
{
    debugln("[mod] Loaded modules:");
    for (uint32_t i = 0; i < mod_count; i++) {
        if (!modules[i].loaded) continue;
        debugln("  [%u] %s @ %p size=%lu init=%p exit=%p",
                i, modules[i].name, modules[i].base, modules[i].size,
                modules[i].init_fn, modules[i].exit_fn);
    }
}
