#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <kernel/initcall.h>
#include <ahci.h>
#include <disk.h>

#define AHCI_TIMEOUT 1000000

static volatile HBA_MEM* hba_mem = NULL;
int ahci_port = -1;

static inline HBA_PORT* port_ptr(int idx) {
    return (HBA_PORT*)&hba_mem->ports[idx];
}

static inline bool wait_clear(volatile uint32_t* reg, uint32_t mask) {
    for (int i = 0; i < AHCI_TIMEOUT; i++) {
        if (!(ahci_arch_mmio_read(reg) & mask))
            return true;
        ahci_arch_cpu_relax();
    }
    return false;
}

static inline bool wait_slot_done(HBA_PORT* port, int slot) {
    for (int i = 0; i < AHCI_TIMEOUT; i++) {
        if (!(ahci_arch_mmio_read(&port->ci) & (1U << slot)))
            return true;
        if (ahci_arch_mmio_read(&port->is) & (1 << 30))
            return false;
        ahci_arch_cpu_relax();
    }
    return false;
}

static bool is_sata(HBA_PORT* port) {
    uint32_t ssts = ahci_arch_mmio_read(&port->ssts);
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    return (det == 3 && ipm == 1);
}

static void stop_cmd(HBA_PORT* port) {
    uint32_t cmd = ahci_arch_mmio_read(&port->cmd);
    cmd &= ~1;
    ahci_arch_mmio_write(&port->cmd, cmd);

    if (!wait_clear(&port->cmd, (1 << 15)))
        return;

    cmd = ahci_arch_mmio_read(&port->cmd);
    cmd &= ~(1 << 4);
    ahci_arch_mmio_write(&port->cmd, cmd);

    wait_clear(&port->cmd, (1 << 14));
}

static void start_cmd(HBA_PORT* port) {
    wait_clear(&port->cmd, (1 << 15));
    uint32_t cmd = ahci_arch_mmio_read(&port->cmd);
    cmd |= (1 << 4) | 1;
    ahci_arch_mmio_write(&port->cmd, cmd);
}

static void port_reset(HBA_PORT* port) {
    uint32_t sctl = ahci_arch_mmio_read(&port->sctl);
    sctl = (sctl & ~0xF) | 1;
    ahci_arch_mmio_write(&port->sctl, sctl);

    for (volatile int i = 0; i < 50000; i++) ahci_arch_cpu_relax();

    sctl = ahci_arch_mmio_read(&port->sctl);
    sctl &= ~0xF;
    ahci_arch_mmio_write(&port->sctl, sctl);

    for (volatile int i = 0; i < 200000; i++) {
        if ((ahci_arch_mmio_read(&port->ssts) & 0x0F) == 3) break;
        ahci_arch_cpu_relax();
    }

    ahci_arch_mmio_write(&port->serr, 0xFFFFFFFF);
}

static void init_port(HBA_PORT* port) {
    stop_cmd(port);

    uintptr_t clb_phys = (uintptr_t)palloc_zero();
    uintptr_t fb_phys  = (uintptr_t)palloc_zero();
    if (!clb_phys || !fb_phys) return;

    ahci_arch_mmio_write((volatile uint32_t*)&port->clb, (uint32_t)(clb_phys & 0xFFFFFFFF));
    ahci_arch_mmio_write((volatile uint32_t*)((uintptr_t)&port->clb + 4), (uint32_t)(clb_phys >> 32));
    ahci_arch_mmio_write((volatile uint32_t*)&port->fb, (uint32_t)(fb_phys & 0xFFFFFFFF));
    ahci_arch_mmio_write((volatile uint32_t*)((uintptr_t)&port->fb + 4), (uint32_t)(fb_phys >> 32));

    HBA_CMD_HEADER* hdr = (HBA_CMD_HEADER*)ahci_arch_phys_to_virt(clb_phys);
    memset(hdr, 0, 1024);

    for (int i = 0; i < 32; i++) {
        uintptr_t ctba_phys = (uintptr_t)palloc_zero();
        if (!ctba_phys) continue;
        hdr[i].ctba  = ctba_phys;
        hdr[i].prdtl = 8;
    }

    ahci_arch_mmio_write(&port->serr, 0xFFFFFFFF);
    ahci_arch_mmio_write(&port->is, 0xFFFFFFFF);
    start_cmd(port);
}

bool ahci_read_sector(int port_num, uint64_t lba, void* buf) {
    if (!hba_mem) return false;

    HBA_PORT* port = port_ptr(port_num);
    ahci_arch_mmio_write(&port->is, 0xFFFFFFFF);

    uint32_t sact = ahci_arch_mmio_read(&port->sact);
    uint32_t ci = ahci_arch_mmio_read(&port->ci);
    uint32_t slots = sact | ci;
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    uint64_t clb = ((uint64_t)ahci_arch_mmio_read((volatile uint32_t*)((uintptr_t)&port->clb + 4)) << 32) |
                    ahci_arch_mmio_read((volatile uint32_t*)&port->clb);
    HBA_CMD_HEADER* hdr = (HBA_CMD_HEADER*)ahci_arch_phys_to_virt(clb);
    memset(&hdr[slot], 0, sizeof(HBA_CMD_HEADER));
    hdr[slot].cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    hdr[slot].w = 0;
    hdr[slot].prdtl = 1;

    HBA_CMD_TBL* tbl = (HBA_CMD_TBL*)ahci_arch_phys_to_virt(hdr[slot].ctba);
    memset(tbl, 0, sizeof(HBA_CMD_TBL));

    uintptr_t paddr = ahci_arch_virt_to_phys(buf);
    if (!paddr) return false;

    tbl->prdt_entry[0].dba  = (uint32_t)(paddr & 0xFFFFFFFF);
    tbl->prdt_entry[0].dbau = (uint32_t)(paddr >> 32);
    tbl->prdt_entry[0].dbc  = 511;
    tbl->prdt_entry[0].i    = 1;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)&tbl->cfis;
    memset(fis, 0, sizeof(FIS_REG_H2D));
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xC8; // READ DMA EXT
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->device = 0x40;
    fis->countl = 1;

    uint32_t timeout = 0;
    while ((ahci_arch_mmio_read(&port->tfd) & (0x80 | 0x08)) && timeout < AHCI_TIMEOUT) {
        timeout++;
        ahci_arch_cpu_relax();
    }
    if (timeout >= AHCI_TIMEOUT) return false;

    ahci_arch_mmio_write(&port->ci, (1U << slot));

    if (!wait_slot_done(port, slot)) return false;
    return !(ahci_arch_mmio_read(&port->is) & (1 << 30));
}

bool ahci_write_sector(int port_num, uint64_t lba, const void* buf) {
    if (!hba_mem) return false;

    HBA_PORT* port = port_ptr(port_num);
    ahci_arch_mmio_write(&port->is, 0xFFFFFFFF);

    uint32_t sact = ahci_arch_mmio_read(&port->sact);
    uint32_t ci = ahci_arch_mmio_read(&port->ci);
    uint32_t slots = sact | ci;
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    uint64_t clb = ((uint64_t)ahci_arch_mmio_read((volatile uint32_t*)((uintptr_t)&port->clb + 4)) << 32) |
                    ahci_arch_mmio_read((volatile uint32_t*)&port->clb);
    HBA_CMD_HEADER* hdr = (HBA_CMD_HEADER*)ahci_arch_phys_to_virt(clb);
    memset(&hdr[slot], 0, sizeof(HBA_CMD_HEADER));
    hdr[slot].cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    hdr[slot].w = 1;
    hdr[slot].prdtl = 1;

    HBA_CMD_TBL* tbl = (HBA_CMD_TBL*)ahci_arch_phys_to_virt(hdr[slot].ctba);
    memset(tbl, 0, sizeof(HBA_CMD_TBL));

    uintptr_t paddr = ahci_arch_virt_to_phys(buf);
    if (!paddr) return false;

    tbl->prdt_entry[0].dba  = (uint32_t)(paddr & 0xFFFFFFFF);
    tbl->prdt_entry[0].dbau = (uint32_t)(paddr >> 32);
    tbl->prdt_entry[0].dbc  = 511;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)&tbl->cfis;
    memset(fis, 0, sizeof(*fis));
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0x35; // WRITE DMA EXT
    fis->device = (1 << 6);
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = 1;

    ahci_arch_mmio_write(&port->ci, (1U << slot));

    if (!wait_slot_done(port, slot)) return false;
    return !(ahci_arch_mmio_read(&port->is) & (1 << 30));
}

int ahci_init(void) {
    debugln("[ahci] Initializing AHCI");

    uint64_t abar = ahci_arch_get_abar();
    if (!abar) return -1;

    hba_mem = (HBA_MEM*)ahci_arch_phys_to_virt((uintptr_t)abar);

    uint32_t ghc = ahci_arch_mmio_read(&hba_mem->ghc);
    ahci_arch_mmio_write(&hba_mem->ghc, ghc | (1 << 31));

    uint32_t pi = ahci_arch_mmio_read(&hba_mem->pi);
    for (int i = 0; i < 32; i++) {
        if (!(pi & (1U << i))) continue;

        HBA_PORT* port = port_ptr(i);
        port_reset(port);

        if (!is_sata(port)) continue;

        init_port(port);
        debug_putchar('.');

        if (ahci_port == -1) {
            ahci_port = i;
        }
    }
    debug_putchar('\n');
    return 0;
}
subsys_initcall(ahci_init);

bool ahci_port_is_present(int port) {
    if (!hba_mem || port < 0 || port >= 32) return false;

    uint32_t ssts = ahci_arch_mmio_read(&hba_mem->ports[port].ssts);
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;

    return (det == 3 && ipm == 1);
}
