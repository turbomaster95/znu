#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

typedef struct {
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    const char* name;
} pci_class_name_t;

static const pci_class_name_t pci_class_names[] = {
    {0x01, 0x01, 0x00, "IDE Controller"},
    {0x01, 0x06, 0x01, "SATA AHCI Controller"},
    {0x02, 0x00, 0x00, "Ethernet Controller"},
    {0x03, 0x00, 0x00, "VGA Compatible Controller"},
    {0x06, 0x00, 0x00, "Host Bridge"},
    {0x06, 0x01, 0x00, "ISA Bridge"},
    {0x0C, 0x03, 0x20, "USB2 (EHCI) Controller"},
    {0x0C, 0x03, 0x30, "USB3 (xHCI) Controller"},
    {0x0C, 0x05, 0x00, "SMBus Controller"},
    {0, 0, 0, NULL}
};

#define PCI_MAX_DEVICES 256
#define PCI_CLASS_STORAGE     0x01
#define PCI_CLASS_NETWORK     0x02
#define PCI_CLASS_DISPLAY     0x03
#define PCI_CLASS_MULTIMEDIA  0x04
#define PCI_CLASS_MEMORY      0x05
#define PCI_CLASS_BRIDGE      0x06
#define PCI_CLASS_COMM        0x07
#define PCI_CLASS_PERIPHERAL  0x08
#define PCI_CLASS_INPUT       0x09
#define PCI_CLASS_DOCKING     0x0A
#define PCI_CLASS_PROCESSOR   0x0B
#define PCI_CLASS_SERIAL      0x0C

#define PCI_SUBCLASS_IDE      0x01
#define PCI_SUBCLASS_SATA     0x06
#define PCI_SUBCLASS_ETHERNET 0x00
#define PCI_SUBCLASS_VGA      0x00
#define PCI_SUBCLASS_USB      0x03
#define PCI_SUBCLASS_PCI      0x04

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
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pci_find_device_by_class(uint8_t class, uint8_t subclass);
void pci_enable_memory_space(pci_device_t* dev);
void pci_enable_msi(pci_device_t* dev, uint8_t vector, void (*handler)(void), const char* name);

#endif
