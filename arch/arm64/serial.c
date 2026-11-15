#include <stdint.h>
#include <arch/serial.h>

#define PL011_BASE 0x09000000UL
#define PL011_DR   (PL011_BASE + 0x00)
#define PL011_FR   (PL011_BASE + 0x18)
#define PL011_IBRD (PL011_BASE + 0x24)
#define PL011_FBRD (PL011_BASE + 0x28)
#define PL011_LCRH (PL011_BASE + 0x2C)
#define PL011_CR   (PL011_BASE + 0x30)
#define PL011_ICR  (PL011_BASE + 0x44)

#define PL011_FR_TXFF (1U << 5)
#define PL011_FR_RXFE (1U << 4)
#define PL011_FR_TXFE (1U << 7)

static inline void mmio_write(uint64_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_read(uint64_t addr) {
    return *(volatile uint32_t *)addr;
}

void arch_serial_init(void) {
    mmio_write(PL011_CR, 0);
    mmio_write(PL011_ICR, 0x7FF);
    mmio_write(PL011_IBRD, 13);
    mmio_write(PL011_FBRD, 1);
    mmio_write(PL011_LCRH, (3U << 5) | (1U << 4));
    mmio_write(PL011_CR, (1U << 0) | (1U << 8) | (1U << 9));
}

int arch_serial_received(void) {
    return (mmio_read(PL011_FR) & PL011_FR_RXFE) == 0;
}

char arch_serial_read(void) {
    while (!arch_serial_received());
    return (char)(mmio_read(PL011_DR) & 0xFF);
}

int arch_serial_read_nonblock(char *buf, int count) {
    int i = 0;
    while (i < count && arch_serial_received()) {
        buf[i++] = (char)(mmio_read(PL011_DR) & 0xFF);
    }
    return i;
}

int arch_serial_tx_empty(void) {
    return (mmio_read(PL011_FR) & PL011_FR_TXFE) != 0;
}

void arch_serial_putc(char c) {
    while (mmio_read(PL011_FR) & PL011_FR_TXFF);
    mmio_write(PL011_DR, (uint8_t)c);
}
