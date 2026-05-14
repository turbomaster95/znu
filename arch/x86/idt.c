#include <idt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pi.h>
#include <lapic.h>
#include <timekeeper.h>
#include <proc.h>
#include <kernel/display.h>
#include <symbols.h>

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

void old_print_stacktrace(uint64_t* rbp, uint64_t max_frames) {
    debugln("\n--- STACK TRACE ---");
    
    for (uint64_t i = 0; i < max_frames; i++) {
        if (!rbp) {
            debugln("  (rbp is null, stopping)");
            break;
        }
        
        uintptr_t rbp_val = (uintptr_t)rbp;
        if (rbp_val < 0xffff800000000000 || rbp_val > 0xffffffffffffffff) {
            debugln("  (rbp %p out of range, stopping)", (void*)rbp);
            break;
        }

        if (rbp_val & 0x7) {
            debugln("  (rbp %p misaligned, stopping)", (void*)rbp);
            break;
        }

        uintptr_t saved_rbp;
        uintptr_t rip;
        
        saved_rbp = rbp[0];
        rip = rbp[1];

        debugln("  #%lu  rip=%p  rbp=%p", i, (void*)rip, (void*)rbp);

        if (saved_rbp == 0) {
            debugln("  (saved rbp is null, done)");
            break;
        }
        
        if (saved_rbp <= rbp_val) {
            debugln("  (saved rbp %p <= current %p, stopping)", 
                    (void*)saved_rbp, (void*)rbp);
            break;
        }

        rbp = (uint64_t*)saved_rbp;
    }
}

registers_t* k_exception_handler(registers_t *regs) {
    uint8_t int_no = regs->int_no;

    if (int_no < 32) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

        debugln("\n--- CRITICAL CPU EXCEPTION: %d ---", (int)int_no);
        debugln("Error Code: 0x%x | RIP: %p | RSP: %p",
                (unsigned)regs->err_code, (void*)regs->rip, (void*)regs->rsp);

        if (int_no == 14) {
            debugln("Faulting Address: %p", (void*)cr2);
            
            if (regs->rip >= 0xffff800000000000 && regs->rip < 0xffffffffffffffff) {
                uint8_t *instruction = (uint8_t*)regs->rip;
                debugln("Opcode at RIP: %02x %02x %02x %02x", 
                        instruction[0], instruction[1], instruction[2], instruction[3]);
            } else {
                debugln("RIP is invalid, cannot read opcode");
            }
        }

        print_stacktrace((uint64_t*)regs->rbp, 12);

        __asm__ volatile("cli");
        while (1) {
            __asm__ volatile("hlt");
        }
    } else if (int_no == LAPIC_TIMER_VECTOR) {
        timer_ticks++;
        lapic_timer_fired = true;

        timekeeper_on_tick();

        regs = scheduler(regs);

        lapic_eoi();
    } else if (int_no == 33) {
	    uint8_t status = inb(0x64);
	    if (status & 1) {
        	uint8_t scancode = inb(0x60);

	        keyboard_handle_scancode(scancode);
    	    }
	    outb(0x20, 0x20); // ACK the irq always

	    lapic_eoi();
    } else if (int_no >= 32 && int_no <= 47 && int_no != 33) {
        // Slave PIC
        if (int_no >= 40) {
            outb(0xA0, 0x20);
        }

        // Master PIC
        outb(0x20, 0x20);
    }

    return regs;
}

void idt_init() {
    memset(idt, 0, sizeof(struct idt_entry) * 256);

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, isr_ptr_table[i]);
    }

    debugln("[idt] Set all IDT gates");

    idtr_instance.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtr_instance.base  = (uint64_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtr_instance));

    debugln("[idt] Ran lidt!");

    pic_remap();

    outb(0x21, 0xFD);

    outb(0xA1, 0xFF);

    debugln("[idt] IDT initialized.");
}

