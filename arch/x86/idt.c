#include <idt.h>
#include <stdlib.h> // for outb
#include <stdio.h>
#include <string.h>
#include <pi.h>
#include <lapic.h>
#include <timekeeper.h>
#include <proc.h>
#include <kernel/tty.h>

extern void hcf(void);
struct idt_entry idt[256] __attribute__((aligned(16)));
struct idtr idtr_instance;
extern void keyboard_handle_scancode(uint8_t scancode);

/* Externs from the ASM file */
extern void isr0(void);
extern void isr32(void); 
extern void* isr_ptr_table[];

extern volatile uint64_t timer_ticks;
extern volatile bool lapic_timer_fired;
extern bool krnl_init_done;
extern bool vmm_ready;
extern volatile bool screen_lock;

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

static int frame_count = 0;

registers_t* k_exception_handler(registers_t *regs) {
    uint8_t int_no = regs->int_no;

    // 1. Handle Critical CPU Exceptions (0-31)
    if (int_no < 32) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

        debugln("\n--- CRITICAL CPU EXCEPTION: %d ---", int_no);
        debugln("Error Code: 0x%x | RIP: %p | RSP: %p", regs->err_code, (void*)regs->rip, (void*)regs->rsp);
        
        if (int_no == 14) {
            debugln("Faulting Address: %p", (void*)cr2);
        }
        
        // Block forever - system is unstable
        hcf();
    }

    if (int_no == 32) {
        timer_ticks++;
        timekeeper_on_tick();
        regs = scheduler(regs);
        if (++frame_count % 16 == 0) {
           if (vmm_ready && term_buffer && !screen_lock) {
             blit_window(term_x, term_y, TERM_W, TERM_H, term_buffer);
           }
        }
    } 
    else if (int_no == 33) {
    //    debugln("[idt] Keyboard IRQ");
        keyboard_handle_scancode(inb(0x60));
    }
    else if (int_no == 48) {
        lapic_timer_fired = true;
        regs = scheduler(regs);
    }
    
    if (int_no >= 32 && int_no < 48) {
        if (int_no >= 40) {
            outb(0xA0, 0x20); // Slave PIC
        }
        outb(0x20, 0x20);     // Master PIC
    }

    if (int_no >= 32) {
        lapic_eoi();
    }
    
    return regs;
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

    outb(0x21, 0xFC); 
    outb(0xA1, 0xFF);
}

