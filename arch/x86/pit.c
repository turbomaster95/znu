#include <stdint.h>
#include <stdlib.h>
#include <prelude.h>

volatile uint64_t timer_ticks = 0;

PERFORM void pit_init(uint32_t frequency) {
    debugln("[pit] Setting frequency to %u Hz", frequency);
    // Standard PIT frequency is 1193182 Hz
    uint32_t divisor = 1193182 / frequency;

    // 0x36 = Channel 0, LSB/MSB, Mode 3 (Square Wave), Binary
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

PERFORM uint16_t read_pit_count(void) {
    uint16_t count = 0;
    
    // Disable interrupts to ensure the low/high byte read is atomic
    // (Optional but recommended if this is used during runtime)
    
    outb(0x43, 0x00); // Latch Channel 0
    count = inb(0x40);
    count |= (inb(0x40) << 8);
    
    return count;
}

PERFORM void msleep(uint32_t ms) {
    uint64_t target = timer_ticks + ms;
    while (timer_ticks < target) {
        __asm__ volatile("hlt"); 
    }
}
