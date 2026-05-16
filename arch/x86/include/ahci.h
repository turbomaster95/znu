#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <page.h>

extern uint64_t* kernel_pml4;

#define AHCI_MAX_PORTS 32

typedef volatile struct {
    uint64_t clb;
    uint64_t fb;

    uint32_t is;
    uint32_t ie;
    uint32_t cmd;

    uint32_t reserved0;

    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;

    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed)) HBA_PORT;

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;

    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];

    HBA_PORT ports[32];
} __attribute__((packed)) HBA_MEM;

typedef struct {
    uint8_t fis_type;

    uint8_t pmport : 4;
    uint8_t reserved0 : 3;
    uint8_t c : 1;

    uint8_t command;
    uint8_t featurel;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;

    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;

    uint8_t reserved1[4];
} __attribute__((packed)) FIS_REG_H2D;

typedef struct {
    uint32_t cfl : 5;
    uint32_t a : 1;
    uint32_t w : 1;
    uint32_t p : 1;

    uint32_t r : 1;
    uint32_t b : 1;
    uint32_t c : 1;
    uint32_t reserved0 : 1;

    uint32_t pmp : 4;
    uint32_t prdtl : 16;

    uint32_t prdbc;

    uint64_t ctba;

    uint32_t reserved1[4];
} __attribute__((packed)) HBA_CMD_HEADER;

typedef struct {
    uint32_t dba;       // Data Base Address (bits 31:0)
    uint32_t dbau;      // Data Base Address Upper (bits 63:32)
    uint32_t reserved0; // Reserved
    uint32_t dbc:22;    // Byte Count
    uint32_t reserved1:9;
    uint32_t i:1;       // Interrupt
} __attribute__((packed)) HBA_PRDT_ENTRY;

typedef struct {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];

    HBA_PRDT_ENTRY prdt_entry[8];
} __attribute__((packed)) HBA_CMD_TBL;

extern int ahci_port;

void ahci_init(void);
bool ahci_read_sector(int port_num, uint64_t lba, void* buf);
bool ahci_write_sector(int port_num, uint64_t lba, void* buf);
bool ahci_port_is_present(int port);

static inline uintptr_t virt_to_phys(void* ptr) {
    uintptr_t vaddr = (uintptr_t)ptr;
    
    if (vaddr >= hhdm_offset && vaddr < 0xffffffff80000000) {
        return vaddr - hhdm_offset;
    }
    
    return vmm_virt_to_phys(kernel_pml4, vaddr);
}

static inline void* phys_to_virt(uintptr_t phys) {
    return PHYS_TO_VIRT(phys);
}

#endif
