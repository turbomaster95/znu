#include <stdint.h>
#include <stdlib.h> // for outb/inb

#define COM1 0x3F8

void serial_init() {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (38400 baud)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int serial_received() {
    return inb(COM1 + 5) & 1;
}

char serial_read() {
    while (serial_received() == 0);
    return inb(COM1);
}

int serial_read_nonblock(char* buf, int count) {
    int i = 0;
    while (i < count && serial_received()) {
        buf[i++] = inb(COM1);
    }
    return i;
}
