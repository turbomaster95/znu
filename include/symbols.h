#ifndef _SYMBOLS_H
#define _SYMBOLS_H

#include <stdint.h>

typedef struct {
    uint64_t address;
    const char *name;
} kernel_symbol_t;

typedef struct {
    const char *name;
    uint64_t base_addr;
    uint64_t offset;
} symbol_info_t;

symbol_info_t symbol_lookup(uint64_t addr);
void print_stacktrace(uint64_t *rbp, uint64_t max_frames);
void symbols_init(void);
uint64_t sym_get_addr(const char* name);

#endif
