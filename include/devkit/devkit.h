#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void devkit_init(void);

void devkit_publish_pci_device(uint8_t bus, uint8_t slot, uint8_t func,
                               uint16_t vendor_id, uint16_t device_id,
                               uint32_t bar0, uint8_t irq);

void devkit_dump_registry(void);

#ifdef __cplusplus
}
#endif
