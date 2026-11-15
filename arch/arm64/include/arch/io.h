#ifndef _ARCH_ARM64_IO_H
#define _ARCH_ARM64_IO_H

#include <stdint.h>

#define PORT_MAP_MAX  64

struct port_map {
    uint16_t port;
    uint64_t base;
};

extern struct port_map port_map_table[];
extern int port_map_count;

static inline uint64_t port_to_addr(uint16_t port)
{
    for (int i = 0; i < port_map_count; i++) {
        if (port_map_table[i].port == port)
            return port_map_table[i].base;
    }
    return 0;
}

static inline void outb(uint16_t port, uint8_t val) {
    *(volatile uint8_t *)port_to_addr(port) = val;
}

static inline uint8_t inb(uint16_t port) {
    return *(volatile uint8_t *)port_to_addr(port);
}

static inline void outw(uint16_t port, uint16_t val) {
    *(volatile uint16_t *)port_to_addr(port) = val;
}

static inline uint16_t inw(uint16_t port) {
    return *(volatile uint16_t *)port_to_addr(port);
}

static inline void outl(uint16_t port, uint32_t val) {
    *(volatile uint32_t *)port_to_addr(port) = val;
}

static inline uint32_t inl(uint16_t port) {
    return *(volatile uint32_t *)port_to_addr(port);
}

static inline void insb(uint16_t port, void *addr, uint32_t cnt) {
    volatile uint8_t *dst = (volatile uint8_t *)addr;
    volatile uint8_t *src = (volatile uint8_t *)port_to_addr(port);
    for (uint32_t i = 0; i < cnt; i++)
        dst[i] = src[i];
}

static inline void outsb(uint16_t port, const void *addr, uint32_t cnt) {
    const volatile uint8_t *src = (const volatile uint8_t *)addr;
    volatile uint8_t *dst = (volatile uint8_t *)port_to_addr(port);
    for (uint32_t i = 0; i < cnt; i++)
        dst[i] = src[i];
}

static inline void insw(uint16_t port, void *addr, uint32_t cnt) {
    volatile uint16_t *dst = (volatile uint16_t *)addr;
    volatile uint16_t *src = (volatile uint16_t *)port_to_addr(port);
    for (uint32_t i = 0; i < cnt; i++)
        dst[i] = src[i];
}

static inline void outsw(uint16_t port, const void *addr, uint32_t cnt) {
    const volatile uint16_t *src = (const volatile uint16_t *)addr;
    volatile uint16_t *dst = (volatile uint16_t *)port_to_addr(port);
    for (uint32_t i = 0; i < cnt; i++)
        dst[i] = src[i];
}

static inline void insl(uint16_t port, void *addr, uint32_t cnt) {
    volatile uint32_t *dst = (volatile uint32_t *)addr;
    volatile uint32_t *src = (volatile uint32_t *)port_to_addr(port);
    for (uint32_t i = 0; i < cnt; i++)
        dst[i] = src[i];
}

static inline void outsl(uint16_t port, const void *addr, uint32_t cnt) {
    const volatile uint32_t *src = (const volatile uint32_t *)addr;
    volatile uint32_t *dst = (volatile uint32_t *)port_to_addr(port);
    for (uint32_t i = 0; i < cnt; i++)
        dst[i] = src[i];
}

static inline void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi) {
    (void)msr; (void)lo; (void)hi;
}

static inline uint64_t rdmsr(uint32_t msr) {
    (void)msr;
    return 0;
}

static inline void write_cr4(uint64_t cr4) { (void)cr4; }
static inline uint64_t read_cr4(void) { return 0; }
static inline void disable_smap(void) {}
static inline void enable_smap(void) {}

#endif
