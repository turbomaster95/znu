#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <stdio.h>

extern char __ksyms_start[];
extern char __ksyms_end[];

#define MAX_SYMS 8192

typedef struct {
    uint64_t addr;
    char *name;
} sym_t;

static sym_t sym_table[MAX_SYMS];
static uint32_t sym_count = 0;
static bool sym_initialized = false;

static uint64_t hex_to_u64(const char *s, int len) {
    uint64_t v = 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    }
    return v;
}

static void parse_symbols(char *start, char *end) {
    char *p = start;
    
    while (p < end && sym_count < MAX_SYMS) {
        while (p < end && (*p == '\n' || *p == ' ' || *p == '\t' || *p == '\0')) p++;
        if (p >= end) break;
        
        if (p + 16 > end) break;
        
        uint64_t addr = hex_to_u64(p, 16);
        p += 16;
        
        while (p < end && *p == ' ') p++;
        
        char *name_start = p;
        while (p < end && *p != '\n' && *p != '\0') p++;
        size_t name_len = p - name_start;
        
        if (name_len == 0) continue;
        
        char *name = kmalloc(name_len + 1);
        if (!name) break;
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';
        
        sym_table[sym_count].addr = addr;
        sym_table[sym_count].name = name;
        sym_count++;
        
        if (p < end && (*p == '\n' || *p == '\0')) p++;
    }
}

void symbols_init(void) {
    if (sym_initialized) return;
    sym_initialized = true;
    
    uint64_t section_size = (uint64_t)(__ksyms_end - __ksyms_start);
    
    if (section_size == 0 || __ksyms_start == __ksyms_end) {
        debugln("[sym] No .ksyms section");
        return;
    }
    
    debugln("[sym] .ksyms at %p, size %lu bytes", __ksyms_start, section_size);
    
    /* Check if section has actual data (not all zeros) */
    bool has_data = false;
    for (uint64_t i = 0; i < section_size && i < 64; i++) {
        if (__ksyms_start[i] != 0) {
            has_data = true;
            break;
        }
    }
    
    if (!has_data) {
        debugln("[sym] .ksyms section is empty (not populated by embsym)");
        return;
    }
    
    parse_symbols(__ksyms_start, __ksyms_end);
    debugln("[sym] Loaded %u symbols", sym_count);
}

const char *sym_lookup(uint64_t addr, uint64_t *offset_out) {
    if (sym_count == 0) {
        if (offset_out) *offset_out = 0;
        return "???";
    }
    
    uint64_t best = 0;
    uint64_t best_dist = ~0ULL;
    
    for (uint32_t i = 0; i < sym_count; i++) {
        if (sym_table[i].addr <= addr) {
            uint64_t dist = addr - sym_table[i].addr;
            if (dist < best_dist) {
                best_dist = dist;
                best = i;
            }
        }
    }
    
    if (offset_out) *offset_out = best_dist;
    return sym_table[best].name;
}

void print_stacktrace(uint64_t *rbp, uint64_t max) {
    debugln("\n--- STACK TRACE ---");
    
    for (uint64_t i = 0; i < max; i++) {
        if (!rbp || (uintptr_t)rbp < 0xffff800000000000 || ((uintptr_t)rbp & 7))
            break;
        
        uint64_t rip = rbp[1];
        uint64_t next = rbp[0];

        if (rip < 0xffffffff80000000) { 
            debugln("  #%lu  %p  <invalid/user RIP>", i, (void*)rip);
            break; 
        }

	uint64_t off;        
        const char *name = sym_lookup(rip, &off);

        if (off)
            debugln("  #%lu  %p  <%s+0x%lx>", i, (void*)rip, name, off);
        else
            debugln("  #%lu  %p  <%s>", i, (void*)rip, name);

        if (next == 0 || next <= (uintptr_t)rbp) break;
        rbp = (uint64_t*)next;
    }
}
