#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

#include <page.h>
#include <sync.h>
#include <tsc.h>
#include <pci.h>
#include <arch/io.h>
#include <devkit/devkit.h>

#include <tendril/glue.h>
#include <tendrilg.h>

extern void *palloc_contig(size_t size);

typedef struct {
    uint64_t raw_phys;
} tendril_dma_header_t;

static inline void tendril_memory_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}

void znu_tendril_init(znu_tendril_ctx_t *ctx, pci_device_t *dev, volatile void *mmio) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->pci = dev;
    ctx->io = dev && dev->bar_type[4] == PCI_BAR_IO;
    ctx->io_base = ctx->io ? (uint16_t)dev->bar_base[4] : 0;
    ctx->mmio = (volatile uint8_t *)mmio;
}

void *tendril_glue_alloc(size_t size) {
    if (!size) return NULL;
    return kmalloc(size);
}

void tendril_glue_free(void *ptr, size_t size) {
    (void)size;
    if (ptr) kfree(ptr);
}

void *tendril_glue_dma_alloc(void *ctx, size_t size, size_t align, bool dma32, uint64_t *dma) {
    (void)ctx;
    if (!dma || !size) return NULL;
    if (!align) align = sizeof(void *);
    if ((align & (align - 1)) != 0) return NULL;
    if (align < sizeof(void *)) align = sizeof(void *);
    if (size > SIZE_MAX - align - sizeof(tendril_dma_header_t)) return NULL;

    size_t total = size + align - 1 + sizeof(tendril_dma_header_t);
    void *raw = palloc_contig(total);
    if (!raw) return NULL;

    uint64_t raw_phys = (uint64_t)(uintptr_t)raw;
    uint64_t start_phys = raw_phys + sizeof(tendril_dma_header_t);
    uint64_t aligned_phys = (start_phys + align - 1) & ~((uint64_t)align - 1);

    if (aligned_phys < start_phys || aligned_phys > UINT64_MAX - size) {
        pfree(raw);
        return NULL;
    }

    if (dma32 && (aligned_phys >= 0x100000000ULL || size > 0x100000000ULL - aligned_phys)) {
        pfree(raw);
        return NULL;
    }

    void *cpu = PHYS_TO_VIRT(aligned_phys);
    tendril_dma_header_t *header = (tendril_dma_header_t *)((uintptr_t)cpu - sizeof(tendril_dma_header_t));
    header->raw_phys = raw_phys;

    memset(cpu, 0, size);
    *dma = aligned_phys;
    return cpu;
}

void tendril_glue_dma_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    if (!ptr) return;

    tendril_dma_header_t *header = (tendril_dma_header_t *)((uintptr_t)ptr - sizeof(tendril_dma_header_t));
    uint64_t raw_phys = header->raw_phys;
    pfree((void *)(uintptr_t)raw_phys);
}

size_t tendril_glue_dma_map(void *ctx, void *ptr, size_t size, int dir, uint64_t *dma) {
    (void)ctx;
    (void)ptr;
    (void)size;
    (void)dir;
    if (dma) *dma = 0;

    // Use DMA bounce buffers until Znu has a verified virtual-to-physical mapper.
    return 0;
}

void tendril_glue_dma_unmap(void *ctx, void *ptr, size_t size, int dir, uint64_t dma) {
    (void)ctx;
    (void)ptr;
    (void)size;
    (void)dir;
    (void)dma;
}

void tendril_glue_dma_barrier(void) {
    tendril_memory_barrier();
}

uint16_t tendril_glue_read16(void *ctx, uint32_t offset) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc) return 0;

    if (hc->io) return inw((uint16_t)(hc->io_base + offset));
    if (!hc->mmio) return 0;

    uint16_t value = *(volatile uint16_t *)(hc->mmio + offset);
    tendril_memory_barrier();
    return value;
}

uint32_t tendril_glue_read32(void *ctx, uint32_t offset) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc) return 0;

    if (hc->io) return inl((uint16_t)(hc->io_base + offset));
    if (!hc->mmio) return 0;

    uint32_t value = *(volatile uint32_t *)(hc->mmio + offset);
    tendril_memory_barrier();
    return value;
}

void tendril_glue_write16(void *ctx, uint32_t offset, uint16_t value) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc) return;

    tendril_memory_barrier();
    if (hc->io) {
        outw((uint16_t)(hc->io_base + offset), value);
        return;
    }
    if (hc->mmio) *(volatile uint16_t *)(hc->mmio + offset) = value;
    tendril_memory_barrier();
}

void tendril_glue_write32(void *ctx, uint32_t offset, uint32_t value) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc) return;

    tendril_memory_barrier();
    if (hc->io) {
        outl((uint16_t)(hc->io_base + offset), value);
        return;
    }
    if (hc->mmio) *(volatile uint32_t *)(hc->mmio + offset) = value;
    tendril_memory_barrier();
}

uint8_t tendril_glue_pci_read8(void *ctx, uint16_t offset) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc || !hc->pci || offset > 0xFF) return 0;
    return pci_read_byte(hc->pci->bus, hc->pci->slot, hc->pci->func, (uint8_t)offset);
}

uint16_t tendril_glue_pci_read16(void *ctx, uint16_t offset) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc || !hc->pci || offset > 0xFE) return 0;
    return pci_read_word(hc->pci->bus, hc->pci->slot, hc->pci->func, (uint8_t)offset);
}

uint32_t tendril_glue_pci_read32(void *ctx, uint16_t offset) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc || !hc->pci || offset > 0xFC) return 0;
    return pci_read_dword(hc->pci->bus, hc->pci->slot, hc->pci->func, (uint8_t)offset);
}

void tendril_glue_pci_write8(void *ctx, uint16_t offset, uint8_t value) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc || !hc->pci || offset > 0xFF) return;

    uint8_t aligned = (uint8_t)(offset & ~3U);
    uint32_t old = pci_read_dword(hc->pci->bus, hc->pci->slot, hc->pci->func, aligned);
    uint32_t shift = (offset & 3U) * 8U;
    uint32_t updated = (old & ~(0xFFU << shift)) | ((uint32_t)value << shift);
    pci_write_dword(hc->pci->bus, hc->pci->slot, hc->pci->func, aligned, updated);
}

void tendril_glue_pci_write16(void *ctx, uint16_t offset, uint16_t value) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc || !hc->pci || offset > 0xFE) return;
    pci_write_word(hc->pci->bus, hc->pci->slot, hc->pci->func, (uint8_t)offset, value);
}

void tendril_glue_pci_write32(void *ctx, uint16_t offset, uint32_t value) {
    znu_tendril_ctx_t *hc = ctx;
    if (!hc || !hc->pci || offset > 0xFC) return;
    pci_write_dword(hc->pci->bus, hc->pci->slot, hc->pci->func, (uint8_t)offset, value);
}

bool tendril_glue_pci_find_read8(void *ctx, uint16_t vendor, uint16_t device, uint16_t offset, uint8_t *value) {
    (void)ctx;
    if (!value || offset > 0xFF) return false;

    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t *dev = &pci_devices[i];
        if (!dev->present || dev->vendor_id != vendor || dev->device_id != device) continue;
        *value = pci_read_byte(dev->bus, dev->slot, dev->func, (uint8_t)offset);
        return true;
    }

    return false;
}

uint64_t tendril_glue_time_ns(void) {
    uint64_t frequency = tsc_get_frequency();
    if (!frequency) return 0;
    uint64_t ticks = tsc_read();
    return (ticks / frequency) * 1000000000ULL + ((ticks % frequency) * 1000000000ULL) / frequency;
}

void tendril_glue_lock(void *ctx) {
    (void)ctx;
}

void tendril_glue_unlock(void *ctx) {
    (void)ctx;
}

void tendril_glue_log(int level, const char *message) {
    if (!message) return;

    switch (level) {
        case TENDRIL_LOG_ERROR: p_error("tendril", "%s", message); break;
        case TENDRIL_LOG_WARN:  p_warn("tendril", "%s", message); break;
        case TENDRIL_LOG_INFO:  p_info("tendril", "%s", message); break;
        case TENDRIL_LOG_DEBUG: p_debug("tendril", "%s", message); break;
        case TENDRIL_LOG_TRACE: p_debug("tendril", "%s", message); break;
        default: p_debug("tendril", "%s", message); break;
    }
}
