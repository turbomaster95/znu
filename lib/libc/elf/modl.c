#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <errno.h>
#include <stdio.h>
#include <symbols.h>
#include <kernel/module.h>

static int parse_modinfo(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data);
static int parse_export_table(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data,
                               uint16_t *sec_idx, size_t *sec_off, size_t nalloc);
static int parse_param_table(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data,
                              uint16_t *sec_idx, size_t *sec_off, size_t nalloc);
static int resolve_dependencies(module_t *mod);
static int apply_params(module_t *mod, const char *args);
static void find_module_hooks(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data,
                               uint16_t *sec_idx, size_t *sec_off, size_t nalloc);
static uint64_t resolve_symbol_full(module_t *mod, const char *name,
                                     Elf64_Sym **out_sym, module_t **out_owner);

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
        debugln("[mod] Not ET_REL (type=%u)", hdr->e_type);
        return -1;
    }
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

uint64_t mod_resolve_symbol(const char *name, module_t *requester)
{
    uint64_t kaddr = sym_get_addr(name);
    if (kaddr) return kaddr;

    for (uint32_t i = 0; i < mod_count; i++) {
        module_t *m = &modules[i];
        if (m->state != MOD_STATE_LIVE) continue;
        if (m == requester) continue;

        for (uint32_t j = 0; j < m->export_count; j++) {
            if (strcmp(m->exports[j].name, name) == 0) {
                mod_get(m);
                return m->exports[j].addr;
            }
        }
    }
    return 0;
}

static uint64_t resolve_symbol_full(module_t *mod, const char *name,
                                     Elf64_Sym **out_sym, module_t **out_owner)
{
    /* Check module's own local symbols first */
    if (mod->symtab && mod->strtab) {
        for (uint32_t i = 0; i < mod->sym_count; i++) {
            Elf64_Sym *s = &mod->symtab[i];
            if (s->st_shndx != SHN_UNDEF && s->st_shndx != SHN_ABS) {
                const char *sname = mod->strtab + s->st_name;
                if (strcmp(sname, name) == 0) {
                    if (out_sym) *out_sym = s;
                    if (out_owner) *out_owner = mod;
                    return 0;  /* caller must add section offset */
                }
            }
        }
    }

    /* External: kernel or other modules */
    uint64_t addr = mod_resolve_symbol(name, mod);
    if (addr) {
        if (out_sym) *out_sym = NULL;
        if (out_owner) *out_owner = NULL;
    }
    return addr;
}

static int apply_rela(module_t *mod, Elf64_Rela *rela, uint64_t symval,
                       size_t sec_offset)
{
    uint64_t *where = (uint64_t *)((uintptr_t)mod->base + sec_offset + rela->r_offset);
    uint64_t S = symval, A = rela->r_addend, P = (uint64_t)where;

    switch (ELF64_R_TYPE(rela->r_info)) {
    case R_X86_64_64:       *where = S + A; break;
    case R_X86_64_PC32:
    case R_X86_64_PLT32:    *(uint32_t *)where = (uint32_t)(S + A - P); break;
    case R_X86_64_32:       *(uint32_t *)where = (uint32_t)(S + A); break;
    case R_X86_64_32S:      *(int32_t *)where  = (int32_t)(S + A); break;
    default:
        debugln("[mod] Unknown reloc type %lu", ELF64_R_TYPE(rela->r_info));
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
                symval = mod_resolve_symbol(symname, mod);
                if (!symval) {
                    debugln("[mod] Unresolved external: %s", symname);
                    return -1;
                }
            } else if (sym->st_shndx == SHN_ABS) {
                symval = sym->st_value;
            } else if (sym->st_shndx < hdr->e_shnum) {
                size_t soff = 0;
                for (size_t j = 0; j < nalloc; j++)
                    if (sec_idx[j] == sym->st_shndx) { soff = sec_off[j]; break; }
                symval = (uint64_t)mod->base + soff + sym->st_value;
            } else {
                return -1;
            }

            if (apply_rela(mod, &rela[r], symval, target_off) < 0) return -1;
        }
    }
    return 0;
}

static int parse_modinfo(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type != SHT_PROGBITS) continue;

        const char *name = NULL;
        /* Find section name via shstrtab */
        if (hdr->e_shstrndx != SHN_UNDEF && hdr->e_shstrndx < hdr->e_shnum) {
            const char *shstrtab = (const char *)((uintptr_t)hdr + shdr[hdr->e_shstrndx].sh_offset);
            name = shstrtab + shdr[i].sh_name;
        }
        if (!name || strcmp(name, ".modinfo") != 0) continue;

        const char *data = (const char *)(elf_data + shdr[i].sh_offset);
        size_t len = shdr[i].sh_size;
        const char *p = data;

        while (p < data + len) {
            const char *end = p;
            while (end < data + len && *end != '\0') end++;
            size_t line_len = end - p;

            /* Parse "key=value" */
            const char *eq = NULL;
            for (size_t k = 0; k < line_len; k++) {
                if (p[k] == '=') { eq = p + k; break; }
            }

            if (eq) {
                size_t key_len = eq - p;
                const char *val = eq + 1;
                size_t val_len = line_len - key_len - 1;

                if (key_len == 4 && strncmp(p, "name", 4) == 0) {
                    size_t n = val_len < MOD_NAME_LEN-1 ? val_len : MOD_NAME_LEN-1;
                    memcpy(mod->name, val, n); mod->name[n] = '\0';
                } else if (key_len == 11 && strncmp(p, "description", 11) == 0) {
                    size_t n = val_len < MOD_DESC_LEN-1 ? val_len : MOD_DESC_LEN-1;
                    memcpy(mod->desc, val, n); mod->desc[n] = '\0';
                } else if (key_len == 6 && strncmp(p, "author", 6) == 0) {
                    size_t n = val_len < MOD_AUTHOR_LEN-1 ? val_len : MOD_AUTHOR_LEN-1;
                    memcpy(mod->author, val, n); mod->author[n] = '\0';
                } else if (key_len == 7 && strncmp(p, "license", 7) == 0) {
                    size_t n = val_len < MOD_LICENSE_LEN-1 ? val_len : MOD_LICENSE_LEN-1;
                    memcpy(mod->license, val, n); mod->license[n] = '\0';
                } else if (key_len == 7 && strncmp(p, "version", 7) == 0) {
                    size_t n = val_len < MOD_VERSION_LEN-1 ? val_len : MOD_VERSION_LEN-1;
                    memcpy(mod->version, val, n); mod->version[n] = '\0';
                } else if (key_len == 9 && strncmp(p, "depends", 9) == 0) {
                    /* Parse comma-separated dependencies */
                    const char *d = val;
                    while (*d && mod->dep_count < MAX_MOD_DEPS) {
                        while (*d == ' ' || *d == ',') d++;
                        const char *start = d;
                        while (*d && *d != ',') d++;
                        size_t dlen = d - start;
                        if (dlen > 0 && dlen < MOD_NAME_LEN) {
                            char *dep = kmalloc(dlen + 1);
                            if (dep) {
                                memcpy(dep, start, dlen);
                                dep[dlen] = '\0';
                                mod->depends[mod->dep_count++] = dep;
                            }
                        }
                    }
                } else if (key_len == 5 && strncmp(p, "alias", 5) == 0) {
                    if (mod->alias_count < MAX_MOD_ALIASES) {
                        char *alias = kmalloc(val_len + 1);
                        if (alias) {
                            memcpy(alias, val, val_len);
                            alias[val_len] = '\0';
                            mod->aliases[mod->alias_count++] = alias;
                        }
                    }
                }
            }

            p = end + 1;
        }
        return 0;  /* found and parsed .modinfo */
    }
    return 0;  /* no .modinfo is OK */
}

static int parse_export_table(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data,
                               uint16_t *sec_idx, size_t *sec_off, size_t nalloc)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type != SHT_PROGBITS) continue;

        const char *name = NULL;
        if (hdr->e_shstrndx != SHN_UNDEF && hdr->e_shstrndx < hdr->e_shnum) {
            const char *shstrtab = (const char *)((uintptr_t)hdr + shdr[hdr->e_shstrndx].sh_offset);
            name = shstrtab + shdr[i].sh_name;
        }
        if (!name || strcmp(name, "__ksymtab") != 0) continue;

        /* Each entry is: { const char *name; void *addr; } */
        typedef struct {
            const char *name;
            void *addr;
        } ksymtab_entry_t;

        ksymtab_entry_t *entries = (ksymtab_entry_t *)(elf_data + shdr[i].sh_offset);
        size_t nentries = shdr[i].sh_size / sizeof(ksymtab_entry_t);

        for (size_t e = 0; e < nentries && mod->export_count < MAX_EXPORTED_SYMS; e++) {
            /* entries[e].name and .addr are currently section-relative (ET_REL).
             * We need to relocate them to absolute addresses. */
            const char *sym_name = entries[e].name;
            void *sym_addr = entries[e].addr;

            /* Find which section these pointers belong to */
            for (size_t j = 0; j < nalloc; j++) {
                uintptr_t sec_start = (uintptr_t)mod->base + sec_off[j];
                uintptr_t sec_end = sec_start + shdr[sec_idx[j]].sh_size;

                /* Check if sym_name falls in this section */
                if ((uintptr_t)sym_name >= (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset) &&
                    (uintptr_t)sym_name < (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset + shdr[sec_idx[j]].sh_size)) {
                    /* Convert to loaded address */
                    uintptr_t offset = (uintptr_t)sym_name - (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset);
                    sym_name = (const char *)(sec_start + offset);
                }

                if ((uintptr_t)sym_addr >= (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset) &&
                    (uintptr_t)sym_addr < (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset + shdr[sec_idx[j]].sh_size)) {
                    uintptr_t offset = (uintptr_t)sym_addr - (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset);
                    sym_addr = (void *)(sec_start + offset);
                }
            }

            mod->exports[mod->export_count].name = sym_name;
            mod->exports[mod->export_count].addr = (uint64_t)sym_addr;
            mod->export_count++;
            debugln("[mod] Export: %s @ %p", sym_name, sym_addr);
        }
        return 0;
    }
    return 0;
}

static int parse_param_table(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data,
                              uint16_t *sec_idx, size_t *sec_off, size_t nalloc)
{
    Elf64_Shdr *shdr = (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);

    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type != SHT_PROGBITS) continue;

        const char *name = NULL;
        if (hdr->e_shstrndx != SHN_UNDEF && hdr->e_shstrndx < hdr->e_shnum) {
            const char *shstrtab = (const char *)((uintptr_t)hdr + shdr[hdr->e_shstrndx].sh_offset);
            name = shstrtab + shdr[i].sh_name;
        }
        if (!name || strcmp(name, "__param") != 0) continue;

        mod_param_t *params = (mod_param_t *)(elf_data + shdr[i].sh_offset);
        size_t nparams = shdr[i].sh_size / sizeof(mod_param_t);

        for (size_t p = 0; p < nparams && mod->param_count < MAX_MOD_PARAMS; p++) {
            /* Copy parameter descriptor */
            memcpy(&mod->params[mod->param_count], &params[p], sizeof(mod_param_t));

            /* Fix up .addr to point into loaded module memory */
            void *param_addr = params[p].addr;
            for (size_t j = 0; j < nalloc; j++) {
                uintptr_t sec_start = (uintptr_t)mod->base + sec_off[j];
                uintptr_t sec_data_start = (uintptr_t)(elf_data + shdr[sec_idx[j]].sh_offset);
                uintptr_t sec_data_end = sec_data_start + shdr[sec_idx[j]].sh_size;

                if ((uintptr_t)param_addr >= sec_data_start &&
                    (uintptr_t)param_addr < sec_data_end) {
                    uintptr_t offset = (uintptr_t)param_addr - sec_data_start;
                    mod->params[mod->param_count].addr = (void *)(sec_start + offset);
                    break;
                }
            }

            mod->param_count++;
            debugln("[mod] Param: %s type=%u addr=%p",
                    mod->params[mod->param_count-1].name,
                    mod->params[mod->param_count-1].type,
                    mod->params[mod->param_count-1].addr);
        }
        return 0;
    }
    return 0;
}


static int resolve_dependencies(module_t *mod)
{
    for (uint32_t i = 0; i < mod->dep_count; i++) {
        const char *depname = mod->depends[i];
        debugln("[mod] Checking dependency: %s", depname);

        module_t *dep = mod_find(depname);
        if (!dep) {
            debugln("[mod] Dependency '%s' not loaded", depname);
            return -1;
        }
        if (dep->state != MOD_STATE_LIVE) {
            debugln("[mod] Dependency '%s' not live (state=%d)", depname, dep->state);
            return -1;
        }
        /* Bump refcount on dependency so it stays loaded */
        mod_get(dep);
        debugln("[mod] Dependency '%s' resolved", depname);
    }
    return 0;
}

static int apply_params(module_t *mod, const char *args)
{
    if (!args) return 0;

    const char *p = args;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        const char *key = p;
        while (*p && *p != '=' && *p != ' ') p++;
        size_t key_len = p - key;

        if (*p != '=') {
            while (*p && *p != ' ') p++;
            continue;
        }
        p++;

        const char *val = p;
        while (*p && *p != ' ') p++;
        size_t val_len = p - val;

        for (uint32_t i = 0; i < mod->param_count; i++) {
            if (strlen(mod->params[i].name) != key_len) continue;
            if (strncmp(mod->params[i].name, key, key_len) != 0) continue;

            switch (mod->params[i].type) {
            case PARAM_TYPE_INT: {
                int64_t num = 0;
                for (size_t k = 0; k < val_len; k++) {
                    if (val[k] >= '0' && val[k] <= '9')
                        num = num * 10 + (val[k] - '0');
                }
                *(int *)mod->params[i].addr = (int)num;
                debugln("[mod] Param %s = %d", mod->params[i].name, (int)num);
                break;
            }
            case PARAM_TYPE_BOOL: {
                bool b = (val_len == 4 && strncmp(val, "true", 4) == 0) ||
                         (val_len == 1 && val[0] == '1') ||
                         (val_len == 2 && strncmp(val, "on", 2) == 0);
                *(bool *)mod->params[i].addr = b;
                debugln("[mod] Param %s = %s", mod->params[i].name, b ? "true" : "false");
                break;
            }
            case PARAM_TYPE_STRING:
            case PARAM_TYPE_CHARP: {
                char *str = kmalloc(val_len + 1);
                if (str) {
                    memcpy(str, val, val_len);
                    str[val_len] = '\0';
                    *(char **)mod->params[i].addr = str;
                    debugln("[mod] Param %s = %s", mod->params[i].name, str);
                }
                break;
            }
            }
            break;
        }
    }
    return 0;
}

static void find_module_hooks(module_t *mod, const Elf64_Ehdr *hdr, uint8_t *elf_data,
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

void mod_get(module_t *mod)
{
    if (!mod) return;
    mod->refcount++;
    debugln("[mod] %s refcount -> %d", mod->name, mod->refcount);
}

void mod_put(module_t *mod)
{
    if (!mod) return;
    mod->refcount--;
    debugln("[mod] %s refcount -> %d", mod->name, mod->refcount);
}

module_t *load_kernel_module(const char *name, uint8_t *elf_data, size_t len,
                              const char *args)
{
    if (!elf_data || len < sizeof(Elf64_Ehdr)) return NULL;
    if (mod_count >= MAX_MODULES) { debugln("[mod] Module table full"); return NULL; }

    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;
    if (elf_validate_reloc(hdr) < 0) return NULL;

    symbols_init();

    /* Calculate layout for SHF_ALLOC sections */
    uint16_t *sec_idx = NULL;
    size_t   *sec_off = NULL;
    size_t    nalloc = 0;
    size_t    total = calc_alloc_size(hdr, &sec_idx, &sec_off, &nalloc);
    if (!total || !sec_idx) { kfree(sec_idx); kfree(sec_off); return NULL; }

    /* Allocate and copy sections */
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

    /* Allocate module descriptor */
    module_t *mod = &modules[mod_count++];
    memset(mod, 0, sizeof(*mod));
    mod->base = base;
    mod->size = total;
    mod->state = MOD_STATE_LOADING;
    mod->refcount = 1;  /* held by being loaded */
    mod->ehdr = hdr;

    size_t nlen = strlen(name);
    if (nlen >= MOD_NAME_LEN) nlen = MOD_NAME_LEN - 1;
    memcpy(mod->name, name, nlen);
    mod->name[nlen] = '\0';

    /* Parse metadata, exports, params from original ELF */
    parse_modinfo(mod, hdr, elf_data);
    parse_export_table(mod, hdr, elf_data, sec_idx, sec_off, nalloc);
    parse_param_table(mod, hdr, elf_data, sec_idx, sec_off, nalloc);

    /* Apply load-time arguments */
    if (args && apply_params(mod, args) < 0) {
        debugln("[mod] Failed to apply parameters");
        goto fail;
    }

    /* Resolve declared dependencies */
    if (resolve_dependencies(mod) < 0) {
        debugln("[mod] Dependency resolution failed");
        goto fail;
    }

    /* Find symbol table in original ELF (not loaded) */
    for (uint16_t i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            mod->symtab = (Elf64_Sym *)(elf_data + shdr[i].sh_offset);
            mod->sym_count = shdr[i].sh_size / sizeof(Elf64_Sym);
            uint16_t stridx = shdr[i].sh_link;
            if (stridx < hdr->e_shnum) {
                mod->strtab = (const char *)(elf_data + shdr[stridx].sh_offset);
            }
            break;
        }
    }

    /* Process relocations */
    if (process_relocations(mod, hdr, sec_idx, sec_off, nalloc) < 0) {
        debugln("[mod] Relocation failed");
        goto fail;
    }

    /* Find init/exit hooks */
    find_module_hooks(mod, hdr, elf_data, sec_idx, sec_off, nalloc);

    kfree(sec_idx);
    kfree(sec_off);
    sec_idx = NULL;
    sec_off = NULL;

    debugln("[mod] Loaded '%s' base=%p size=%lu init=%p exit=%p exports=%u",
            mod->name, mod->base, mod->size, mod->init_fn, mod->exit_fn,
            mod->export_count);

    /* Call init */
    if (mod->init_fn) {
        debugln("[mod] Calling %s::module_init", mod->name);
        mod->state = MOD_STATE_LOADING;
        int ret = ((int (*)(void))mod->init_fn)();
        mod->init_ret = ret;
        if (ret != 0) {
            debugln("[mod] %s::module_init returned %d", mod->name, ret);
            mod->state = MOD_STATE_GOING;
            goto fail;
        }
    }

    mod->state = MOD_STATE_LIVE;
    debugln("[mod] %s is now LIVE", mod->name);
    return mod;

fail:
    if (sec_idx) kfree(sec_idx);
    if (sec_off) kfree(sec_off);
    for (uint32_t i = 0; i < mod->dep_count; i++) {
        module_t *dep = mod_find(mod->depends[i]);
        if (dep) mod_put(dep);
    }
    if (mod->base) kfree(mod->base);
    mod->state = MOD_STATE_UNLOADED;
    mod_count--;
    return NULL;
}

int unload_kernel_module(module_t *mod)
{
    if (!mod || mod->state == MOD_STATE_UNLOADED) return -1;

    if (mod->refcount > 1) {
        debugln("[mod] %s refcount=%d, cannot unload", mod->name, mod->refcount);
        return -1;
    }

    mod->state = MOD_STATE_GOING;

    if (mod->exit_fn) {
        debugln("[mod] Calling %s::module_exit", mod->name);
        ((void (*)(void))mod->exit_fn)();
    }

    for (uint32_t i = 0; i < mod->dep_count; i++) {
        module_t *dep = mod_find(mod->depends[i]);
        if (dep) {
            mod_put(dep);
            kfree(mod->depends[i]);
        }
    }

    for (uint32_t i = 0; i < mod->alias_count; i++) {
        kfree(mod->aliases[i]);
    }

    if (mod->base) kfree(mod->base);
    mod->base = NULL;
    mod->state = MOD_STATE_UNLOADED;
    mod->refcount = 0;

    debugln("[mod] %s unloaded", mod->name);
    return 0;
}

module_t *mod_find(const char *name)
{
    for (uint32_t i = 0; i < mod_count; i++) {
        if (modules[i].state != MOD_STATE_UNLOADED &&
            strcmp(modules[i].name, name) == 0)
            return &modules[i];
    }
    return NULL;
}

module_t *mod_find_by_alias(const char *alias)
{
    for (uint32_t i = 0; i < mod_count; i++) {
        if (modules[i].state != MOD_STATE_LIVE) continue;
        for (uint32_t j = 0; j < modules[i].alias_count; j++) {
            if (strcmp(modules[i].aliases[j], alias) == 0)
                return &modules[i];
        }
    }
    return NULL;
}

void mod_info(const char *name)
{
    module_t *mod = mod_find(name);
    if (!mod) {
        debugln("[mod] Module '%s' not found", name);
        return;
    }

    debugln("=== Module: %s ===", mod->name);
    debugln("  Description: %s", mod->desc[0] ? mod->desc : "(none)");
    debugln("  Author:      %s", mod->author[0] ? mod->author : "(none)");
    debugln("  License:     %s", mod->license[0] ? mod->license : "(none)");
    debugln("  Version:     %s", mod->version[0] ? mod->version : "(none)");
    debugln("  State:       %s",
            mod->state == MOD_STATE_LIVE ? "LIVE" :
            mod->state == MOD_STATE_LOADING ? "LOADING" :
            mod->state == MOD_STATE_GOING ? "GOING" : "UNLOADED");
    debugln("  Refcount:    %d", mod->refcount);
    debugln("  Base:        %p", mod->base);
    debugln("  Size:        %lu", mod->size);
    debugln("  Init:        %p", mod->init_fn);
    debugln("  Exit:        %p", mod->exit_fn);
    debugln("  Exports:     %u", mod->export_count);
    debugln("  Parameters:  %u", mod->param_count);
    debugln("  Dependencies:");
    for (uint32_t i = 0; i < mod->dep_count; i++)
        debugln("    - %s", mod->depends[i]);
    debugln("  Aliases:");
    for (uint32_t i = 0; i < mod->alias_count; i++)
        debugln("    - %s", mod->aliases[i]);
}

void mod_list(void)
{
    debugln("[mod] Loaded modules:");
    for (uint32_t i = 0; i < mod_count; i++) {
        module_t *m = &modules[i];
        if (m->state == MOD_STATE_UNLOADED) continue;
        debugln("  [%u] %-16s state=%s ref=%d size=%lu exports=%u init=%s",
                i, m->name,
                m->state == MOD_STATE_LIVE ? "LIVE" :
                m->state == MOD_STATE_LOADING ? "LOAD" :
                m->state == MOD_STATE_GOING ? "GOING" : "DEAD",
                m->refcount, m->size, m->export_count,
                m->init_fn ? "yes" : "no");
    }
}
