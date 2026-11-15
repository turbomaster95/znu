#ifndef ARCH_PAGE_H
#define ARCH_PAGE_H

#include <stdint.h>

#define ARM64_PTE_VALID          (1ULL << 0)
#define ARM64_PTE_TABLE          (1ULL << 1)
#define ARM64_PTE_ATTR_NORMAL    (0ULL << 2)
#define ARM64_PTE_ATTR_DEVICE    (1ULL << 2)
#define ARM64_PTE_AP_RW_EL1      (0ULL << 6)
#define ARM64_PTE_AP_RW_EL0      (1ULL << 6)
#define ARM64_PTE_AP_RO_EL1      (2ULL << 6)
#define ARM64_PTE_AP_RO_EL0      (3ULL << 6)
#define ARM64_PTE_NS             (1ULL << 5)
#define ARM64_PTE_AF             (1ULL << 10)
#define ARM64_PTE_SH_INNER       (3ULL << 8)
#define ARM64_PTE_PXN            (1ULL << 53)
#define ARM64_PTE_UXN            (1ULL << 54)
#define ARM64_PTE_CONTIG         (1ULL << 52)
#define ARM64_PTE_nG             (1ULL << 11)

#define ARM64_PTE_ADDR_MASK      0x0000FFFFFFFFF000ULL

#define ARM64_MAIR_ATTR_DEVICE_nGnRnE  0x00
#define ARM64_MAIR_ATTR_NORMAL_WB      0xFF
#define ARM64_MAIR_IDX_DEVICE          0
#define ARM64_MAIR_IDX_NORMAL          1

#define ARM64_L0_INDEX(addr) (((uint64_t)(addr) >> 39) & 0x1FF)
#define ARM64_L1_INDEX(addr) (((uint64_t)(addr) >> 30) & 0x1FF)
#define ARM64_L2_INDEX(addr) (((uint64_t)(addr) >> 21) & 0x1FF)
#define ARM64_L3_INDEX(addr) (((uint64_t)(addr) >> 12) & 0x1FF)

#endif
