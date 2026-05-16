#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <pci.h>
#include <ahci.h>
#include <disk.h>

extern uint64_t hhdm_offset;

#define AHCI_TIMEOUT 1000000

static HBA_MEM* hba_mem = NULL;
volatile HBA_MEM* g_abar = NULL;

int ahci_port = -1;

static inline HBA_PORT* port_ptr(int idx) {
    return &hba_mem->ports[idx];
}

static inline bool wait_clear(volatile uint32_t* reg, uint32_t mask) {
    for (int i = 0; i < AHCI_TIMEOUT; i++) {
        if (!(*reg & mask))
            return true;
        asm volatile("pause");
    }
    return false;
}

static inline bool wait_slot_done(HBA_PORT* port, int slot) {
    for (int i = 0; i < AHCI_TIMEOUT; i++) {
        if (!(port->ci & (1U << slot)))
            return true;

        if (port->is & (1 << 30))
            return false;

        asm volatile("pause");
    }
    return false;
}

static bool is_sata(HBA_PORT* port) {
    uint32_t ssts = port->ssts;

    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;

    return (det == 3 && ipm == 1);
}

static void stop_cmd(HBA_PORT* port) {
    port->cmd &= ~1;

    if (!wait_clear(&port->cmd, (1 << 15)))
        return;

    port->cmd &= ~(1 << 4);

    wait_clear(&port->cmd, (1 << 14));
}

static void start_cmd(HBA_PORT* port) {
    wait_clear(&port->cmd, (1 << 15));

    port->cmd |= (1 << 4);
    port->cmd |= 1;
}

static void port_reset(HBA_PORT* port) {
    port->sctl = (port->sctl & ~0xF) | 1;

    for (volatile int i = 0; i < 50000; i++)
        asm volatile("pause");

    port->sctl &= ~0xF;

    for (volatile int i = 0; i < 200000; i++) {
        if ((port->ssts & 0x0F) == 3)
            break;
        asm volatile("pause");
    }

    port->serr = 0xFFFFFFFF;
}

static void init_port(HBA_PORT* port) {
    stop_cmd(port);

    uintptr_t clb_phys = (uintptr_t)palloc_zero();
    uintptr_t fb_phys  = (uintptr_t)palloc_zero();

    if (!clb_phys || !fb_phys)
        return;

    port->clb = clb_phys;
    port->fb  = fb_phys;

    HBA_CMD_HEADER* hdr =
        (HBA_CMD_HEADER*)phys_to_virt(clb_phys);

    memset(hdr, 0, 1024);

    for (int i = 0; i < 32; i++) {
        uintptr_t ctba_phys = (uintptr_t)palloc_zero();
        if (!ctba_phys)
            continue;

        hdr[i].ctba  = ctba_phys;
        hdr[i].prdtl = 8;
    }

    port->serr = 0xFFFFFFFF;
    port->is   = 0xFFFFFFFF;

    start_cmd(port);
}


bool ahci_read_sector(int port_num, uint64_t lba, void* buf) {
    if (!hba_mem) {
        debugln("[ahci] FATAL: hba_mem is NULL");
        return false;
    }

    HBA_PORT* port = port_ptr(port_num);
    //debugln("[ahci] Start read: Port=%d, LBA=%u, Buf(virt)=%p", port_num, (uint32_t)lba, buf);

    port->is = 0xFFFFFFFF;
    //debugln("[ahci] Interrupt status cleared. PxTFD=%x", port->tfd);

    uint32_t slots = port->sact | port->ci;
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        debugln("[ahci] ERROR: No command slots available!");
        return false;
    }
    //debugln("[ahci] Selected slot: %d", slot);

    HBA_CMD_HEADER* hdr = (HBA_CMD_HEADER*)phys_to_virt(port->clb);
    memset(&hdr[slot], 0, sizeof(HBA_CMD_HEADER));

    hdr[slot].cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // 5 DWORDS
    hdr[slot].w = 0;      // Read
    hdr[slot].prdtl = 1;  // One PRDT entry
    //debugln("[ahci] Header set: CLB(phys)=%x, CFL=%d", port->clb, hdr[slot].cfl);

    HBA_CMD_TBL* tbl = (HBA_CMD_TBL*)phys_to_virt(hdr[slot].ctba);
    memset(tbl, 0, sizeof(HBA_CMD_TBL));

    uintptr_t paddr = virt_to_phys(buf);
    if (!paddr) {
        debugln("[ahci] ERROR: Translation failed for buffer %p", buf);
        return false;
    }
    //debugln("[ahci] Translation: VMM(%p) -> PMM(%p)", buf, (void*)paddr);

    tbl->prdt_entry[0].dba  = (uint32_t)(paddr & 0xFFFFFFFF);
    tbl->prdt_entry[0].dbau = (uint32_t)(paddr >> 32);
    tbl->prdt_entry[0].dbc  = 511; // 512 bytes (0-indexed)
    tbl->prdt_entry[0].i    = 1;   // Interrupt bit
    //debugln("[ahci] PRDT set: DBA=%x, DBAU=%x, DBC=%d", 
    //        tbl->prdt_entry[0].dba, tbl->prdt_entry[0].dbau, tbl->prdt_entry[0].dbc);

    FIS_REG_H2D* fis = (FIS_REG_H2D*)&tbl->cfis;
    memset(fis, 0, sizeof(FIS_REG_H2D));

    fis->fis_type = 0x27;
    fis->c = 1;           // Command bit
    fis->command = 0xC8;  // READ DMA EXT (48-bit)

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->device = 0x40;   // LBA mode
    fis->countl = 1;      // Read 1 sector
    fis->counth = 0;      // 16-bit count (High byte zero)
    //debugln("[ahci] FIS set: Cmd=0x25, LBA=%u, Device=0x40", (uint32_t)lba);

    uint32_t timeout = 0;
    while ((port->tfd & (0x80 | 0x08)) && timeout < AHCI_TIMEOUT) {
        timeout++;
        asm volatile("pause");
    }
    if (timeout >= AHCI_TIMEOUT) {
        debugln("[ahci] ERROR: Port Busy/DRQ timeout before command. PxTFD=%x", port->tfd);
        return false;
    }

    //debugln("[ahci] Triggering command via PxCI...");
    port->ci = (1U << slot);

    if (!wait_slot_done(port, slot)) {
        debugln("[ahci] ERROR: Command Timeout! PxIS=%x, PxTFD=%x, PxSERR=%x", 
                port->is, port->tfd, port->serr);
        return false;
    }

    if (port->is & (1 << 30)) {
        debugln("[ahci] ERROR: Task File Error detected! PxTFD=%x", port->tfd);
        return false;
    }

    //debugln("[ahci] Success: Sector read complete.");
    return true;
}

bool ahci_write_sector(int port_num, uint64_t lba, void* buf) {
    if (!hba_mem)
        return false;

    HBA_PORT* port = port_ptr(port_num);

    port->is = 0xFFFFFFFF;

    uint32_t slots = port->sact | port->ci;

    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return false;

    HBA_CMD_HEADER* hdr =
        (HBA_CMD_HEADER*)phys_to_virt(port->clb);

    memset(&hdr[slot], 0, sizeof(HBA_CMD_HEADER));

    hdr[slot].cfl  = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    hdr[slot].w    = 1;
    hdr[slot].prdtl = 1;

    HBA_CMD_TBL* tbl =
        (HBA_CMD_TBL*)phys_to_virt(hdr[slot].ctba);

    memset(tbl, 0, sizeof(HBA_CMD_TBL));

    tbl->prdt_entry[0].dba = virt_to_phys(buf);
    tbl->prdt_entry[0].dbc = 511;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)&tbl->cfis;
    memset(fis, 0, sizeof(*fis));

    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0x35;

    fis->device = (1 << 6);

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = 1;

    port->ci = (1U << slot);

    if (!wait_slot_done(port, slot))
        return false;

    return !(port->is & (1 << 30));
}


void ahci_init(void) {
    debugln("[ahci] Initializing AHCI");

    pci_device_t* dev =
        pci_find_class(0x01, 0x06, 0x01);

    if (!dev)
        return;

    pci_enable_busmaster(dev);

    uint32_t lo = pci_read_dword(dev->bus, dev->slot, dev->func, 0x24);
    uint32_t hi = pci_read_dword(dev->bus, dev->slot, dev->func, 0x28);

    uint64_t abar =
        ((uint64_t)hi << 32) | (lo & 0xFFFFFFF0);

    g_abar = (volatile HBA_MEM*)(uintptr_t)(abar + hhdm_offset);
    hba_mem = (HBA_MEM*)(uintptr_t)(abar + hhdm_offset);

    hba_mem->ghc |= (1 << 31);

    uint32_t pi = hba_mem->pi;

    for (int i = 0; i < 32; i++) {

        if (!(pi & (1U << i)))
            continue;

        HBA_PORT* port = port_ptr(i);

        port_reset(port);

        if (!is_sata(port))
            continue;

        init_port(port);

        // disk_register_ahci_port(i);
	debug_putchar('.');

        if (ahci_port == -1)
            ahci_port = i;
    }
    debug_putchar('\n');
}

bool ahci_port_is_present(int port) {
    if (!g_abar || port < 0 || port >= 32)
        return false;

    uint32_t ssts = g_abar->ports[port].ssts;

    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;

    return (det == 3 && ipm == 1);
}
