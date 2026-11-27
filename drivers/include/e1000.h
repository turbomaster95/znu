#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stddef.h>
#include <pci.h>

#define E1000_VENDOR_ID     0x8086
#define E1000_DEVICE_ID     0x100E  /* 82540EM - the chip QEMU emulates */
#define E1000_DEVICE_ID_2   0x153A  /* I219-V (PCH, e1000e family)      */
#define E1000_DEVICE_ID_3   0x10EA  /* 82577LM (PCH, e1000e family)     */

#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008
#define E1000_REG_EEPROM    0x0014  /* EERD */
#define E1000_REG_CTRL_EXT  0x0018
#define E1000_REG_ICR       0x00C0  /* Interrupt Cause Read (read clears) */
#define E1000_REG_ICS       0x00C8  /* Interrupt Cause Set */
#define E1000_REG_IMS       0x00D0  /* Interrupt Mask Set (write 1s) */
#define E1000_REG_IMC       0x00D8  /* Interrupt Mask Clear (write 1s) */
#define E1000_REG_RCTL      0x0100  /* RX control */
#define E1000_REG_TCTL      0x0400  /* TX control */
#define E1000_REG_TIPG      0x0410  /* TX inter-packet gap */
#define E1000_REG_RXDCTL    0x2828
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818
#define E1000_REG_TXDCTL    0x3828
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818
#define E1000_REG_MTA       0x5200  /* multicast table array: 128 x 32-bit */
#define E1000_REG_RAL       0x5400  /* receive address 0 low */
#define E1000_REG_RAH       0x5404  /* receive address 0 high */

#define E1000_CTRL_FD       (1u << 0)   /* full duplex */
#define E1000_CTRL_SLU      (1u << 6)   /* Set Link Up - MUST be set or the
                                           PHY leaves the link down */
#define E1000_CTRL_FRCSPD   (1u << 11)  /* force speed (unused: auto-neg) */
#define E1000_CTRL_FRCDPX   (1u << 12)  /* force duplex (unused: auto-neg) */
#define E1000_CTRL_RST      (1u << 26)  /* global reset, self-clearing */

#define E1000_STATUS_FD     (1u << 0)
#define E1000_STATUS_LU     (1u << 1)

#define E1000_EERD_START            (1u << 0)
#define E1000_EERD_DONE             (1u << 4)   /* 8254x done bit */
#define E1000_EERD_ADDR_SHIFT       8           /* 8254x address shift */
#define E1000_EERD_DONE_ICH         (1u << 1)   /* 8257x/ICH-style done bit */
#define E1000_EERD_ADDR_SHIFT_ICH   2           /* 8257x/ICH-style shift */
#define E1000_EERD_DATA_SHIFT       16
#define E1000_EERD_DATA_MASK        0xFFFF0000u

#define E1000_RCTL_EN       (1u << 1)
#define E1000_RCTL_SBP      (1u << 2)
#define E1000_RCTL_UPE      (1u << 3)   /* unicast promiscuous */
#define E1000_RCTL_MPE      (1u << 4)   /* multicast promiscuous */
#define E1000_RCTL_LPE      (1u << 5)   /* long packet enable (jumbo) */
#define E1000_RCTL_LBM_NO   (0u << 6)
#define E1000_RCTL_LBM_PHY  (3u << 6)
#define E1000_RCTL_RDMTS_HALF    (0u << 8)
#define E1000_RCTL_RDMTS_QUARTER (1u << 8)
#define E1000_RCTL_RDMTS_EIGHTH  (2u << 8)
#define E1000_RCTL_MO_36    (0u << 12)
#define E1000_RCTL_MO_35    (1u << 12)
#define E1000_RCTL_MO_34    (2u << 12)
#define E1000_RCTL_MO_32    (3u << 12)
#define E1000_RCTL_BAM      (1u << 15)  /* broadcast accept */
#define E1000_RCTL_VFE      (1u << 18)
#define E1000_RCTL_CFIEN    (1u << 19)
#define E1000_RCTL_CFI      (1u << 20)
#define E1000_RCTL_DPF      (1u << 22)
#define E1000_RCTL_PMCF     (1u << 23)
#define E1000_RCTL_BSEX     (1u << 25)  /* buffer-size extension */
#define E1000_RCTL_SECRC    (1u << 26)  /* strip CRC before delivery */
/* buffer size when BSEX=0 */
#define E1000_RCTL_SZ_2048  (0u << 16)
#define E1000_RCTL_SZ_1024  (1u << 16)
#define E1000_RCTL_SZ_512   (2u << 16)
#define E1000_RCTL_SZ_256   (3u << 16)

#define E1000_TCTL_EN       (1u << 1)
#define E1000_TCTL_PSP      (1u << 3)
#define E1000_TCTL_SWXOFF   (1u << 22)
#define E1000_TCTL_RTLC     (1u << 24)
#define E1000_TCTL_CT_MASK      0x00000FF0u /* collision threshold field */
#define E1000_TCTL_CT_SHIFT     4
#define E1000_TCTL_COLD_MASK    0x003FF000u /* collision distance field */
#define E1000_TCTL_COLD_SHIFT   12
#define E1000_COLLISION_THRESHOLD   0xF    /* datasheet value */
#define E1000_COLLISION_DISTANCE    63     /* datasheet value (0x3F) */

#define E1000_TIPG_DEFAULT  0x00602008u

#define E1000_CMD_EOP       (1u << 0)
#define E1000_CMD_IFCS      (1u << 1)
#define E1000_CMD_IC        (1u << 2)
#define E1000_CMD_RS        (1u << 3)
#define E1000_CMD_RPS       (1u << 4)
#define E1000_CMD_VLE       (1u << 6)
#define E1000_CMD_IDE       (1u << 7)

/* Descriptor status/error bits */
#define E1000_RXD_STAT_DD   0x01    /* descriptor done */
#define E1000_RXD_STAT_EOP  0x02    /* end of packet */
#define E1000_RXD_ERR_RXE   0x80    /* RX data error (CRC etc.) */
#define E1000_TXD_STAT_DD   0x01    /* descriptor done */

#define E1000_ICR_TXDW      (1u << 0)   /* TX descriptor written back */
#define E1000_ICR_TXQE      (1u << 1)   /* TX queue empty */
#define E1000_ICR_LSC       (1u << 2)   /* link status change */
#define E1000_ICR_RXDMT     (1u << 4)   /* RX desc min threshold hit */
#define E1000_ICR_RXO       (1u << 6)   /* RX overrun */
#define E1000_ICR_RXT0      (1u << 7)   /* RX timer interrupt (ring 0) */

#define E1000_IMS_DEFAULT   (E1000_ICR_TXDW | E1000_ICR_TXQE | \
                             E1000_ICR_LSC  | E1000_ICR_RXDMT | \
                             E1000_ICR_RXO  | E1000_ICR_RXT0)

#define E1000_RAH_AV        (1u << 31)

#define NUM_RX_DESC         256
#define NUM_TX_DESC         256
#define RX_BUFFER_SIZE      2048
#define TX_BUFFER_SIZE      2048

typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} PACKED e1000_rx_desc;

typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} PACKED e1000_tx_desc;

typedef struct {
    uint8_t bus, slot, func;
    uint64_t mem_base;          /* virtual MMIO base (BAR0 + hhdm) */
    uint16_t device_id;
    e1000_rx_desc *rx_ring;
    e1000_tx_desc *tx_ring;
    uint16_t rx_cur;            /* next RX descriptor to poll */
    uint16_t tx_cur;            /* next TX descriptor to fill */
    uint16_t tx_dirty;          /* oldest TX descriptor not yet reaped */
    uint16_t tx_in_flight;      /* TX descriptors owned by the NIC */
    uint8_t mac[6];
    uint8_t has_eeprom;
    uint8_t present;
    uint8_t irq;                /* filled in by the IRQ */
} e1000_device;

int  e1000_init(void);
int  e1000_present(void);
int  e1000_send_packet(void *data, size_t len);
int  e1000_receive_packet(void *buf, size_t buf_size);
void e1000_get_mac_address(uint8_t *mac);
uint32_t e1000_link_up(void);
void e1000_enable_interrupts(void);
void e1000_disable_interrupts(void);
uint32_t e1000_get_interrupt_status(void);
void e1000_handle_interrupt(void);

#endif
