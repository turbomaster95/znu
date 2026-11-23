#ifndef TENDRILGLUE_H
#define TENDRILGLUE_H

#include <stdint.h>
#include <stdbool.h>
#include <pci.h>

typedef struct {
    pci_device_t *pci;
    volatile uint8_t *mmio;
    uint16_t io_base;
    bool io;
} znu_tendril_ctx_t;

void znu_tendril_init(znu_tendril_ctx_t *ctx, pci_device_t *dev, volatile void *mmio);

#endif
