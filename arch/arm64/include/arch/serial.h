#ifndef ARCH_SERIAL_H
#define ARCH_SERIAL_H

void arch_serial_init(void);
int arch_serial_received(void);
char arch_serial_read(void);
int arch_serial_read_nonblock(char *buf, int count);
int arch_serial_tx_empty(void);
void arch_serial_putc(char c);

#endif
