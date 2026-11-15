#include <arch/serial.h>

void serial_init(void) {
    arch_serial_init();
}

int serial_received(void) {
    return arch_serial_received();
}

char serial_read(void) {
    return arch_serial_read();
}

int serial_read_nonblock(char* buf, int count) {
    return arch_serial_read_nonblock(buf, count);
}

int is_transmit_empty(void) {
    return arch_serial_tx_empty();
}

void serial_putchar(char c) {
    arch_serial_putc(c);
}

void serial_write_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(str[i]);
    }
}
