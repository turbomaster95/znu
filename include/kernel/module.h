#ifndef _KERNEL_MODULE_H
#define _KERNEL_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <elf.h>

#define MOD_NAME_LEN        64
#define MOD_DESC_LEN        128
#define MOD_AUTHOR_LEN      64
#define MOD_LICENSE_LEN     32
#define MOD_VERSION_LEN     32
#define MAX_MOD_ALIASES     4
#define MAX_MOD_DEPS        4
#define MAX_MOD_PARAMS      8
#define MAX_EXPORTED_SYMS   64
#define MAX_MODULES         16

typedef enum {
    MOD_STATE_LOADING = 0,
    MOD_STATE_LIVE,
    MOD_STATE_GOING,
    MOD_STATE_UNLOADED
} mod_state_t;

typedef enum {
    PARAM_TYPE_INT = 0,
    PARAM_TYPE_BOOL,
    PARAM_TYPE_STRING,
    PARAM_TYPE_CHARP,
} param_type_t;

typedef struct mod_param {
    char         name[32];
    param_type_t type;
    void        *addr;
    uint64_t     def_val;
    const char  *def_str;
    const char  *desc;
} mod_param_t;

typedef struct ksym_export {
    uint64_t    addr;
    const char *name;
} ksym_export_t;

typedef struct module {
    char        name[MOD_NAME_LEN];
    char        desc[MOD_DESC_LEN];
    char        author[MOD_AUTHOR_LEN];
    char        license[MOD_LICENSE_LEN];
    char        version[MOD_VERSION_LEN];

    void       *base;
    size_t      size;
    mod_state_t state;
    int32_t     refcount;

    void       *init_fn;
    void       *exit_fn;
    int         init_ret;

    ksym_export_t exports[MAX_EXPORTED_SYMS];
    uint32_t      export_count;

    mod_param_t  params[MAX_MOD_PARAMS];
    uint32_t     param_count;

    char        *depends[MAX_MOD_DEPS];
    uint32_t     dep_count;

    char        *aliases[MAX_MOD_ALIASES];
    uint32_t     alias_count;

    Elf64_Sym  *symtab;
    const char *strtab;
    uint32_t    sym_count;
    const Elf64_Ehdr *ehdr;
} module_t;

module_t *load_kernel_module(const char *name, uint8_t *elf_data, size_t len,
                              const char *args);
int       unload_kernel_module(module_t *mod);
module_t *mod_find(const char *name);
module_t *mod_find_by_alias(const char *alias);
void      mod_list(void);
void      mod_info(const char *name);
uint64_t  mod_resolve_symbol(const char *name, module_t *requester);
void      mod_get(module_t *mod);
void      mod_put(module_t *mod);

#define __MODULE_INFO(tag, info) \
    __attribute__((section(".modinfo"), used)) \
    static const char __modinfo_##tag[] = #tag "=" info

#define MODULE_NAME(n)          __MODULE_INFO(name, n)
#define MODULE_DESCRIPTION(d)   __MODULE_INFO(description, d)
#define MODULE_AUTHOR(a)        __MODULE_INFO(author, a)
#define MODULE_LICENSE(l)       __MODULE_INFO(license, l)
#define MODULE_VERSION(v)       __MODULE_INFO(version, v)

#define MODULE_DEPENDS(d) \
    __attribute__((section(".modinfo"), used)) \
    static const char __modinfo_depends[] = "depends=" d

#define MODULE_ALIAS(a) \
    __attribute__((section(".modinfo"), used)) \
    static const char __modinfo_alias_##a[] = "alias=" #a

#define EXPORT_SYMBOL(sym) \
    __attribute__((section("__ksymtab"), used)) \
    static const struct { \
        const char *name; \
        void *addr; \
    } __ksym_##sym = { #sym, (void*)&sym }

#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)

#define __MODULE_PARAM(name_str, var, typeval, desc_str) \
    __attribute__((section("__param"), used)) \
    static mod_param_t __param_##var = { \
        .name = name_str, \
        .type = typeval, \
        .addr = &var, \
        .desc = desc_str \
    }

#define MODULE_PARAM_INT(var, desc)      __MODULE_PARAM(#var, var, PARAM_TYPE_INT, desc)
#define MODULE_PARAM_BOOL(var, desc)     __MODULE_PARAM(#var, var, PARAM_TYPE_BOOL, desc)
#define MODULE_PARAM_STRING(var, desc)   __MODULE_PARAM(#var, var, PARAM_TYPE_STRING, desc)
#define MODULE_PARAM_CHARP(var, desc)    __MODULE_PARAM(#var, var, PARAM_TYPE_CHARP, desc)

#define MODULE_PARAM_INT_NAMED(pname, var, desc)   __MODULE_PARAM(#pname, var, PARAM_TYPE_INT, desc)
#define MODULE_PARAM_BOOL_NAMED(pname, var, desc)  __MODULE_PARAM(#pname, var, PARAM_TYPE_BOOL, desc)

#define MODULE_PARAM(var, type, desc) MODULE_PARAM_##type(var, desc)

#define module_init(fn) \
    int module_init(void) __attribute__((alias(#fn))); \
    int fn(void)

#define module_exit(fn) \
    void module_exit(void) __attribute__((alias(#fn))); \
    void fn(void)

#endif
