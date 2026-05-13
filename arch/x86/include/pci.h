#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;

    uint8_t  header_type;

    uint32_t bar[6];

    bool present;
} pci_device_t;

#define PCI_MAX_DEVICES 256

extern pci_device_t pci_devices[PCI_MAX_DEVICES];
extern int pci_device_count;

uint32_t pci_read_dword(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
);

uint16_t pci_read_word(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
);

uint8_t pci_read_byte(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
);

void pci_write_dword(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset,
    uint32_t value
);

void pci_init(void);

pci_device_t* pci_find_class(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if
);

void pci_enable_busmaster(pci_device_t* dev);

#endif
