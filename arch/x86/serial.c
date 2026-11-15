#include <arch/io.h>

#define COM1 0x3F8

void arch_serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

int arch_serial_received(void) {
    return inb(COM1 + 5) & 1;
}

char arch_serial_read(void) {
    while (arch_serial_received() == 0);
    return inb(COM1);
}

int arch_serial_read_nonblock(char *buf, int count) {
    int i = 0;
    while (i < count && arch_serial_received()) {
        buf[i++] = inb(COM1);
    }
    return i;
}

int arch_serial_tx_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void arch_serial_putc(char c) {
    while (arch_serial_tx_empty() == 0) {
        __asm__ volatile("pause");
    }
    outb(COM1, c);
}
