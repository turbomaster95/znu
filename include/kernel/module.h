#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct module module_t;

module_t *load_kernel_module(const char *name, uint8_t *elf_data, size_t len);
int       unload_kernel_module(module_t *mod);
module_t *mod_find(const char *name);
void      mod_list(void);

#endif
