#ifndef _GDT_H
#define _GDT_H

#include <stdint.h>

void gdt_reload_segments(void);
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
void gdt_set_tss(int num, uint64_t base, uint32_t limit);
void gdt_init();

#endif
