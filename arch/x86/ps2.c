#include <stdint.h>
#include <stdlib.h>

void ps2_init(void) {
    // Wait for input buffer empty
    while (inb(0x64) & 2) {}
    
    // Enable keyboard interrupt (IRQ1)
    // Read configuration byte
    outb(0x64, 0x20);
    while (!(inb(0x64) & 1)) {}
    uint8_t config = inb(0x60);
    
    // Set bit 0 (keyboard interrupt enable) and bit 1 (mouse, optional)
    config |= 1;
    
    // Write back
    outb(0x64, 0x60);
    while (inb(0x64) & 2) {}
    outb(0x60, config);
    
    // Enable keyboard device
    outb(0x64, 0xAE);  // Enable keyboard interface
    
    // Reset keyboard
    while (inb(0x64) & 2) {}
    outb(0x60, 0xFF);  // Reset command
}
