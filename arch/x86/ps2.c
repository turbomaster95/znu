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
    
    // Set bit 0 (keyboard interrupt enable), bit 1 (mouse), and bit 6 (translation)
    config |= 0x41; // Enable IRQ1 and Translation to Set 1
    
    // Write back
    outb(0x64, 0x60);
    while (inb(0x64) & 2) {}
    outb(0x60, config);
    
    // Enable keyboard device
    outb(0x64, 0xAE);  // Enable keyboard interface
    
    // Reset keyboard
    while (inb(0x64) & 2) {}
    outb(0x60, 0xFF);  // Reset command
    
    // Wait for ACK (0xFA) and BAT (0xAA)
    for(int i = 0; i < 1000; i++) {
        if(inb(0x64) & 1) {
            uint8_t res = inb(0x60);
            if(res == 0xAA) break;
        }
    }
}
