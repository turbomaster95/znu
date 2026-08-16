#include <pci.h>
#include <stdlib.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t x86_pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size) {
    uint32_t address = 0x80000000 |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);

    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t value = inl(PCI_CONFIG_DATA);

    if (size == 1) {
        return (value >> ((offset & 3) * 8)) & 0xFF;
    } else if (size == 2) {
        return (value >> ((offset & 2) * 8)) & 0xFFFF;
    }
    return value;
}

static void x86_pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value, uint8_t size) {
    uint32_t address = 0x80000000 |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);

    outl(PCI_CONFIG_ADDRESS, address);

    if (size == 4) {
        outl(PCI_CONFIG_DATA, value);
    } else {
        uint32_t orig = inl(PCI_CONFIG_DATA);
        if (size == 1) {
            uint32_t mask = 0xFF << ((offset & 3) * 8);
            value = (orig & ~mask) | ((value & 0xFF) << ((offset & 3) * 8));
        } else if (size == 2) {
            uint32_t mask = 0xFFFF << ((offset & 2) * 8);
            value = (orig & ~mask) | ((value & 0xFFFF) << ((offset & 2) * 8));
        }
        outl(PCI_CONFIG_DATA, value);
    }
}

static struct pci_ops x86_pci_ops = {
    .read  = x86_pci_read,
    .write = x86_pci_write
};

struct pci_ops* arch_pci_get_ops(void) {
    return &x86_pci_ops;
}
