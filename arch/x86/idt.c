#include <idt.h>
#include <stdlib.h> // for outb
#include <stdio.h>
#include <string.h>
#include <pi.h>
#include <lapic.h>
#include <timekeeper.h>

extern void hcf(void);
struct idt_entry idt[256] __attribute__((aligned(16)));
struct idtr idtr_instance;

/* Externs from the ASM file */
extern void isr0(void);
extern void isr32(void); 
extern void* isr_ptr_table[];

extern volatile uint64_t timer_ticks;
extern volatile bool lapic_timer_fired;
extern bool krnl_init_done;

void idt_set_gate(uint8_t vector, void *isr) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].isr_low    = (uint16_t)addr;
    idt[vector].kernel_cs  = 0x08;
    idt[vector].ist        = 0;
    idt[vector].attributes = 0x8E; // Present, Ring 0, Interrupt Gate
    idt[vector].isr_mid    = (uint16_t)(addr >> 16);
    idt[vector].isr_high   = (uint32_t)(addr >> 32);
    idt[vector].reserved   = 0;
}

void k_exception_handler(registers_t *regs) {
    // 1. Catch Critical CPU Exceptions (Page Faults, GPFs, etc.)
    if (regs->int_no < 32) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        
        debugln("\n--- CRITICAL CPU EXCEPTION ---");
        debugln("Interrupt Number : %d", regs->int_no);
        debugln("Error Code       : 0x%x", regs->err_code);
        if (regs->int_no == 14) { // Page Fault
            debugln("Faulting Address : %p", (void*)cr2);
        }
        hcf(); // Stop the silent loop
    }

    // 2. Hardware Interrupts
    if (regs->int_no >= 32 && regs->int_no < 48) {
        if (regs->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }

    if (regs->int_no >= 32) {
        lapic_eoi();
    }

    if (regs->int_no == 32) {
        timekeeper_on_tick();
        timer_ticks++;
        if (timer_ticks % 1000 == 0) {
           if (!krnl_init_done) {
		   debugln("Tick! %d", timer_ticks);
	   }
	}
        return;
    }

    if (regs->int_no == 48) {
        lapic_timer_fired = true;
        return;
    }
}

void idt_init() {
    memset(idt, 0, sizeof(struct idt_entry) * 256);

    // Set all gates to isr32, then override specific ones
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, isr_ptr_table[i]);
    }

    // Override specific ones
    idt_set_gate(0,  isr0);
    debugln("[idt] Set all idt gates");

    idtr_instance.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtr_instance.base  = (uint64_t)&idt;
    asm volatile ("lidt %0" : : "m"(idtr_instance));
    debugln("[idt] Ran lidt!");

    pic_remap();

    outb(0x21, 0xFD);
    // Unmask IRQ0 (Timer)
    outb(0x21, 0xFE); 
    outb(0xA1, 0xFF); // All masked on slave
}

