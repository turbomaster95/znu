// arch/x86/pit.c
#include <stdint.h>
#include <stdlib.h>
#include <prelude.h>

volatile uint64_t timer_ticks = 0;

PERFORM void pit_init(uint32_t frequency) {
    // The PIT internal clock frequency is 1.193182 MHz
    uint32_t divisor = 1193182 / frequency;

    outb(0x43, 0x34);             // Command byte: Square wave, Rate Generator
    outb(0x40, (uint8_t)(divisor & 0xFF));        // Low byte
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}

PERFORM void msleep(uint32_t ms) {
    uint64_t target = timer_ticks + ms;
    while (timer_ticks < target) {
        __asm__ volatile("hlt"); // Wait for the next interrupt
    }
}

PERFORM uint16_t read_pit_count(void) {
    uint16_t count = 0;

    // Send the latch command for Channel 0
    // Binary 00000000: Channel 0, Latch Count, Mode 0, Binary
    outb(0x43, 0x00);

    // Read low byte then high byte
    count = inb(0x40);          // Low byte
    count |= (inb(0x40) << 8);  // High byte

    return count;
}

