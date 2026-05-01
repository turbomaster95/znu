#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <uacpi/uacpi.h>

// This file contains definitions of functions called by kmain, but who don't necessarily need their own .h files.
// The arch-specific counterpart of this is at arch/x86/include/x86.h

extern uint32_t lapic_ticks_per_ms;
extern uintptr_t stack_top;
extern bool vmm_ready;

void calibrate_lapic_timer_no_irq(void);
int lz4_unframe(const uint8_t* source, uint8_t* dest, size_t input_size, size_t max_output);
void jump_to_usermode(uintptr_t entry, uintptr_t stack);
void draw_kernel_gui(void);
extern uacpi_status init_acpi(void);
extern void kernel_reboot(void);
extern void lapic_timer_test(void);
void parse_cmdline(char *cmdline);
bool boot_get_flag(const char *key);
const char* boot_get_value(const char *key);

#endif