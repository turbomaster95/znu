#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <page.h>
#include <pci.h>
#include <ahci.h>
#include <disk.h>

extern uint64_t* kernel_pml4;

static HBA_MEM* hba_mem = NULL;
volatile HBA_MEM* g_abar = NULL;

int ahci_port = -1;

static inline uintptr_t virt_to_phys(void* ptr) {
    return vmm_virt_to_phys(
        kernel_pml4,
        (uintptr_t)ptr
    );
}

static inline void* phys_to_virt(uintptr_t phys) {
    return PHYS_TO_VIRT(phys);
}

static inline HBA_PORT* ahci_port_ptr(int idx) {
    return &hba_mem->ports[idx];
}

static bool check_sata_drive(HBA_PORT* port) {
    uint32_t ssts = port->ssts;

    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;

    return det == 3 && ipm == 1;
}

static void stop_cmd(HBA_PORT* port) {
    port->cmd &= ~0x01;

    while (port->cmd & (1 << 15))
        asm volatile("pause");

    port->cmd &= ~(1 << 4);

    while (port->cmd & (1 << 14))
        asm volatile("pause");
}

static void start_cmd(HBA_PORT* port) {
    while (port->cmd & (1 << 15))
        asm volatile("pause");

    port->cmd |= (1 << 4);
    port->cmd |= (1 << 0);
}

static void port_reset(HBA_PORT* port) {
    port->sctl = (port->sctl & 0xFFFFFFF0) | 0x01;

    for (volatile int i = 0; i < 100000; i++)
        asm volatile("pause");

    port->sctl &= 0xFFFFFFF0;

    for (volatile int i = 0; i < 5000000; i++) {
        if ((port->ssts & 0x0F) == 3)
            break;

        asm volatile("pause");
    }

    port->serr = 0xFFFFFFFF;
}

static void init_port(HBA_PORT* port) {
    stop_cmd(port);

    // Command list
    uintptr_t clb_phys = (uintptr_t)palloc_zero();
    void* clb = PHYS_TO_VIRT(clb_phys);

    port->clb = clb_phys;

    // FIS receive
    uintptr_t fb_phys = (uintptr_t)palloc_zero();

    void* fb = PHYS_TO_VIRT(fb_phys);

    port->fb = fb_phys;

    debugln(
      "[ahci] clb virt=%p phys=%p",
      clb,
      clb_phys
    );

    debugln(
      "[ahci] fb virt=%p phys=%p",
      fb,
      fb_phys
    );

    HBA_CMD_HEADER* cmd_hdr = (HBA_CMD_HEADER*)PHYS_TO_VIRT(port->clb);

    for (int i = 0; i < 32; i++) {
	uintptr_t ctba_phys = (uintptr_t)palloc_zero();

        void* ctba = PHYS_TO_VIRT(ctba_phys);

        cmd_hdr[i].prdtl = 1;
        cmd_hdr[i].ctba = ctba_phys;
    }

    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    start_cmd(port);
}

bool ahci_read_sector(int port_num, uint64_t lba, void* buf) {
    if (!hba_mem)
        return false;

    HBA_PORT* port = ahci_port_ptr(port_num);

    port->is = 0xFFFFFFFF;

    uint32_t slots = port->sact | port->ci;

    int slot = -1;

    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return false;

    HBA_CMD_HEADER* cmd_hdr = (HBA_CMD_HEADER*)PHYS_TO_VIRT(port->clb);

    HBA_CMD_HEADER* hdr = &cmd_hdr[slot];

    memset(hdr, 0, sizeof(HBA_CMD_HEADER));

    hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    hdr->w = 0;
    hdr->prdtl = 1;

    HBA_CMD_TBL* tbl = phys_to_virt(hdr->ctba);

    memset(tbl, 0, sizeof(HBA_CMD_TBL));

    uintptr_t phys = virt_to_phys(buf);
    if (!phys) return false;

    tbl->prdt_entry[0].dba = phys;
    tbl->prdt_entry[0].dbc = 511;

    FIS_REG_H2D* fis =
        (FIS_REG_H2D*)&tbl->cfis;

    memset(fis, 0, sizeof(FIS_REG_H2D));

    fis->fis_type = 0x27;
    fis->c = 1;

    fis->command = 0x25; // READ DMA EXT

    fis->device = 1 << 6;

    fis->lba0 = (uint8_t)(lba);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = 1;

    int spin = 0;

    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
        asm volatile("pause");
        spin++;
    }

    if (spin >= 1000000)
        return false;

    port->ci = (1U << slot);

    while (1) {
        if (!(port->ci & (1U << slot)))
            break;

        if (port->is & (1 << 30))
            return false;
    }

    return true;
}

bool ahci_write_sector(int port_num, uint64_t lba, void* buf) {
    if (!hba_mem)
        return false;

    HBA_PORT* port = ahci_port_ptr(port_num);

    port->is = 0xFFFFFFFF;

    uint32_t slots = port->sact | port->ci;

    int slot = -1;

    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return false;

    HBA_CMD_HEADER* cmd_hdr =
        (HBA_CMD_HEADER*)(uintptr_t)PHYS_TO_VIRT(port->clb);

    HBA_CMD_HEADER* hdr = &cmd_hdr[slot];

    memset(hdr, 0, sizeof(HBA_CMD_HEADER));

    hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    hdr->w = 1;
    hdr->prdtl = 1;

    HBA_CMD_TBL* tbl =
        (HBA_CMD_TBL*)(uintptr_t)PHYS_TO_VIRT(hdr->ctba);

    memset(tbl, 0, sizeof(HBA_CMD_TBL));

    tbl->prdt_entry[0].dba = (uint64_t)(uintptr_t)buf;
    tbl->prdt_entry[0].dbc = 511;

    FIS_REG_H2D* fis =
        (FIS_REG_H2D*)&tbl->cfis;

    memset(fis, 0, sizeof(FIS_REG_H2D));

    fis->fis_type = 0x27;
    fis->c = 1;

    fis->command = 0x35; // WRITE DMA EXT

    fis->device = 1 << 6;

    fis->lba0 = (uint8_t)(lba);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = 1;

    int spin = 0;

    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
        asm volatile("pause");
        spin++;
    }

    if (spin >= 1000000)
        return false;

    port->ci = (1U << slot);

    while (1) {
        if (!(port->ci & (1U << slot)))
            break;

        if (port->is & (1 << 30))
            return false;
    }

    return true;
}

void ahci_init(void) {
    pci_device_t* dev = pci_find_class(0x01, 0x06, 0x01);

    if (!dev) {
        debugln("[ahci] no controller found");
        return;
    }

    pci_enable_busmaster(dev);


    uint32_t abar_low =
      pci_read_dword(dev->bus, dev->slot, dev->func, 0x24);

    uint32_t abar_high =
      pci_read_dword(dev->bus, dev->slot, dev->func, 0x28);

    uint64_t abar =
        ((uint64_t)abar_high << 32) |
        (abar_low & 0xFFFFFFF0);

    g_abar = (volatile HBA_MEM*)(uintptr_t)(abar + hhdm_offset);

    hba_mem = (HBA_MEM*)(uintptr_t)(abar + hhdm_offset);

    hba_mem->ghc |= (1 << 31);

    debugln("[ahci] controller online");

    uint32_t pi = hba_mem->pi;

    for (int i = 0; i < 32; i++) {
        if (!(pi & (1U << i)))
            continue;

        HBA_PORT* port = ahci_port_ptr(i);

        port_reset(port);

        if (!check_sata_drive(port))
            continue;

        debugln("[ahci] SATA drive on port %d", i);

	disk_register_ahci_port(i);

        init_port(port);

        if (ahci_port == -1)
            ahci_port = i;
    }
}

bool ahci_port_is_present(int port)
{
    if (port < 0 || port >= 32)
        return false;


    extern volatile HBA_MEM* g_abar;

    HBA_MEM* abar = (HBA_MEM*)g_abar;
    if (!abar)
        return false;

    // Check SATA status (ssts) for device presence
    uint32_t ssts = abar->ports[port].ssts;

    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;

    return (det == 3 && ipm == 1); // device present + active
}
