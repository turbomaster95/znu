#include <stdint.h>
#include <stdlib.h>

static void ps2_wait_input(void) {
    while (inb(0x64) & 2);
}

static void ps2_wait_output(void) {
    while (!(inb(0x64) & 1));
}

void ps2_init(void) {

    // Disable devices
    ps2_wait_input();
    outb(0x64, 0xAD);

    ps2_wait_input();
    outb(0x64, 0xA7);

    // Flush output buffer
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    // Read config byte
    ps2_wait_input();
    outb(0x64, 0x20);

    ps2_wait_output();
    uint8_t config = inb(0x60);

    // Enable IRQ1 + translation
    config |= (1 << 0);
    config |= (1 << 6);

    // Write config back
    ps2_wait_input();
    outb(0x64, 0x60);

    ps2_wait_input();
    outb(0x60, config);

    // Enable keyboard interface
    ps2_wait_input();
    outb(0x64, 0xAE);

    // Enable scanning
    ps2_wait_input();
    outb(0x60, 0xF4);

    // Wait for ACK
    ps2_wait_output();
    uint8_t ack = inb(0x60);

    debugln("[ps2] Enable scanning ACK=0x%x", ack);

    debugln("[ps2] Initialized PS/2");
}
