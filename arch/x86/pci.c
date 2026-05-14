#include <pci.h>
#include <stdio.h>
#include <stdlib.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

pci_device_t pci_devices[PCI_MAX_DEVICES];
int pci_device_count = 0;

static void pci_add_device(
    uint8_t bus,
    uint8_t slot,
    uint8_t func
);

static void pci_scan_device(
    uint8_t bus,
    uint8_t slot
);

static void pci_scan_function(
    uint8_t bus,
    uint8_t slot,
    uint8_t func
);

static void pci_scan_bus(uint8_t bus);

const char* pci_get_class_name(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    for (int i = 0; pci_class_names[i].name != NULL; i++) {
        if (pci_class_names[i].class_code == class_code &&
            pci_class_names[i].subclass == subclass) {
            
            return pci_class_names[i].name;
        }
    }
    return "Unknown Device";
}

const char* pci_get_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel Corp";
        case 0x1234: return "Bochs/QEMU";
        case 0x10EC: return "Realtek";
        case 0x1AF4: return "VirtIO";
        default:     return "Unknown Vendor";
    }
}

uint32_t pci_read_dword(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
) {
    uint32_t address =
        0x80000000 |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC);

    outl(PCI_CONFIG_ADDRESS, address);

    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_word(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
) {
    uint32_t value =
        pci_read_dword(bus, slot, func, offset);

    return (value >> ((offset & 2) * 8)) & 0xFFFF;
}

uint8_t pci_read_byte(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
) {
    uint32_t value =
        pci_read_dword(bus, slot, func, offset);

    return (value >> ((offset & 3) * 8)) & 0xFF;
}

void pci_write_dword(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset,
    uint32_t value
) {
    uint32_t address =
        0x80000000 |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC);

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_add_device(
    uint8_t bus,
    uint8_t slot,
    uint8_t func
) {
    if (pci_device_count >= PCI_MAX_DEVICES)
        return;

    uint16_t vendor_id =
        pci_read_word(bus, slot, func, 0x00);

    if (vendor_id == 0xFFFF)
        return;

    pci_device_t* dev =
        &pci_devices[pci_device_count++];

    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;

    dev->vendor_id =
        vendor_id;

    dev->device_id =
        pci_read_word(bus, slot, func, 0x02);

    dev->revision =
        pci_read_byte(bus, slot, func, 0x08);

    dev->prog_if =
        pci_read_byte(bus, slot, func, 0x09);

    dev->subclass =
        pci_read_byte(bus, slot, func, 0x0A);

    dev->class_code =
        pci_read_byte(bus, slot, func, 0x0B);

    dev->header_type =
        pci_read_byte(bus, slot, func, 0x0E);

    dev->bar[0] =
        pci_read_dword(bus, slot, func, 0x10);

    dev->bar[1] =
        pci_read_dword(bus, slot, func, 0x14);

    dev->bar[2] =
        pci_read_dword(bus, slot, func, 0x18);

    dev->bar[3] =
        pci_read_dword(bus, slot, func, 0x1C);

    dev->bar[4] =
        pci_read_dword(bus, slot, func, 0x20);

    dev->bar[5] =
        pci_read_dword(bus, slot, func, 0x24);

    dev->present = true;

    const char* vendor_name = pci_get_vendor_name(dev->vendor_id);
    const char* device_name = pci_get_class_name(dev->class_code, dev->subclass, dev->prog_if);

    debugln(
        "PCI %02X:%02X.%u "
        "class=%02X subclass=%02X progif=%02X "
        "vendor=%04X device=%04X [%s %s]",
        bus,
        slot,
        func,
        dev->class_code,
        dev->subclass,
        dev->prog_if,
        dev->vendor_id,
        dev->device_id,
	vendor_name,
	device_name
    );
}

static void pci_scan_function(
    uint8_t bus,
    uint8_t slot,
    uint8_t func
) {
    pci_add_device(bus, slot, func);
}

static void pci_scan_device(
    uint8_t bus,
    uint8_t slot
) {
    uint16_t vendor =
        pci_read_word(bus, slot, 0, 0x00);

    if (vendor == 0xFFFF)
        return;

    pci_scan_function(bus, slot, 0);

    uint8_t header_type =
        pci_read_byte(bus, slot, 0, 0x0E);

    if (header_type & 0x80) {
        for (uint8_t func = 1; func < 8; func++) {
            vendor =
                pci_read_word(bus, slot, func, 0x00);

            if (vendor != 0xFFFF)
                pci_scan_function(bus, slot, func);
        }
    }
}

static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        pci_scan_device(bus, slot);
    }
}

void pci_init(void) {
    pci_device_count = 0;

    debugln("[pci] scanning...");

    for (uint16_t bus = 0; bus < 256; bus++) {
        pci_scan_bus((uint8_t)bus);
    }

    debugln(
        "[pci] found %d devices",
        pci_device_count
    );
}

pci_device_t* pci_find_class(
    uint8_t class_code,
    uint8_t subclass,
    uint8_t prog_if
) {
    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t* dev =
            &pci_devices[i];

        if (
            dev->class_code == class_code &&
            dev->subclass  == subclass &&
            dev->prog_if   == prog_if
        ) {
            return dev;
        }
    }

    return NULL;
}

void pci_enable_busmaster(
    pci_device_t* dev
) {
    uint16_t cmd =
        pci_read_word(
            dev->bus,
            dev->slot,
            dev->func,
            0x04
        );

    cmd |= (1 << 2);

    uint32_t value =
        pci_read_dword(
            dev->bus,
            dev->slot,
            dev->func,
            0x04
        );

    value &= 0xFFFF0000;
    value |= cmd;

    pci_write_dword(
        dev->bus,
        dev->slot,
        dev->func,
        0x04,
        value
    );
}
