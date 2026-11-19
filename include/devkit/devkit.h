#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVKIT_PCI_BAR_NONE 0
#define DEVKIT_PCI_BAR_MMIO 1
#define DEVKIT_PCI_BAR_IO   2

typedef struct {
    uint64_t base;
    uint64_t size;
    uint8_t type;
    uint8_t width;
    uint8_t prefetchable;
} devkit_pci_bar_t;

void devkit_init(void);

void devkit_publish_pci_device(uint8_t bus, uint8_t slot, uint8_t func, uint16_t vendor_id,
				uint16_t device_id, const devkit_pci_bar_t bars[6], uint8_t irq);

void devkit_dump_registry(void);

#ifdef __cplusplus
}
#endif
