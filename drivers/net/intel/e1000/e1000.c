/**
 * @brief : Intel E1000 network driver with MSI/Legacy interrupts and DMA ring buffers.
 * 
 * This file is a part of the Zen (ZenOS)
 * Operating System, and is released under
 * the terms of the MIT Licensing : Read
 * LICENSE at the root of the repository.
 * 
 * @copyright (c) 2026
 * @author : Rishies2010
 */

#include <e1000.h>
#include <net.h>
#include <page.h>
#include <pci.h>
#include <lapic.h>
#include <idt.h>
#include <stdlib.h>

static e1000_device dev;
static pci_device_t *pci_dev = NULL;
static void *rx_buf_virt[NUM_RX_DESC];
static void *tx_buf_virt[NUM_TX_DESC];
bool msi_capable = false;

static uint32_t e1000_read(uint32_t reg)
{
    uint32_t val = *((volatile uint32_t *)(dev.mem_base + reg));
    return val;
}

static void e1000_write(uint32_t reg, uint32_t value)
{
    *((volatile uint32_t *)(dev.mem_base + reg)) = value;
}

static uint32_t e1000_eeprom_read(uint8_t addr)
{
    uint32_t data = 0;
    uint32_t tmp = 0;

    if (dev.has_eeprom)
    {
        e1000_write(E1000_REG_EEPROM, 1 | (addr << 8));
        while (!((tmp = e1000_read(E1000_REG_EEPROM)) & (1 << 4)))
            ;
    }
    else
    {
        e1000_write(E1000_REG_EEPROM, 1 | (addr << 2));
        while (!((tmp = e1000_read(E1000_REG_EEPROM)) & (1 << 1)))
            ;
    }

    data = (tmp >> 16) & 0xFFFF;
    return data;
}

static uint8_t e1000_detect_eeprom(void)
{
    e1000_write(E1000_REG_EEPROM, 1);
    for (int i = 0; i < 10000; i++)
    {
        uint32_t val = e1000_read(E1000_REG_EEPROM);
        if (val & (1 << 4)) {
            return 1;
        }
    }
    return 0;
}

static void e1000_reset(void)
{
    uint32_t ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | 0x04000000);
    while (e1000_read(E1000_REG_CTRL) & 0x04000000)
        ;
    for (volatile int i = 0; i < 100; i++) ;
}

static void e1000_read_mac(void)
{
    uint32_t mac_low = e1000_read(E1000_REG_RXADDR);
    uint32_t mac_high = e1000_read(E1000_REG_RXADDR2);

    if (mac_low != 0 || mac_high != 0) {
        dev.mac[0] = mac_low & 0xFF;
        dev.mac[1] = (mac_low >> 8) & 0xFF;
        dev.mac[2] = (mac_low >> 16) & 0xFF;
        dev.mac[3] = (mac_low >> 24) & 0xFF;
        dev.mac[4] = mac_high & 0xFF;
        dev.mac[5] = (mac_high >> 8) & 0xFF;
        return;
    }

    if (dev.has_eeprom)
    {
        uint32_t temp = e1000_eeprom_read(0);
        dev.mac[0] = temp & 0xFF; dev.mac[1] = temp >> 8;
        temp = e1000_eeprom_read(1);
        dev.mac[2] = temp & 0xFF; dev.mac[3] = temp >> 8;
        temp = e1000_eeprom_read(2);
        dev.mac[4] = temp & 0xFF; dev.mac[5] = temp >> 8;
    }
}

static void e1000_init_rx(void)
{
    uint64_t ring_phys = (uint64_t)palloc_zero();
    dev.rx_ring = (e1000_rx_desc *)(ring_phys + hhdm_offset);
    memset(dev.rx_ring, 0, NUM_RX_DESC * sizeof(e1000_rx_desc));

    uint64_t buffers_phys = (uint64_t)palloc_zero();
    for (int i = 0; i < NUM_RX_DESC; i++)
    {
        uint64_t buf_phys = buffers_phys + (i * PAGE_SIZE);
        dev.rx_ring[i].addr = buf_phys;
        dev.rx_ring[i].status = 0;
    }

    e1000_write(E1000_REG_RDBAL, (uint32_t)(ring_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_RDBAH, (uint32_t)(ring_phys >> 32));
    e1000_write(E1000_REG_RDLEN, NUM_RX_DESC * sizeof(e1000_rx_desc));
    e1000_write(E1000_REG_RDT, NUM_RX_DESC - 1);
    
    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_BAM;
    e1000_write(E1000_REG_RCTL, rctl);
}

void e1000_init_tx(void)
{
    uint64_t ring_phys = (uint64_t)palloc_zero();
    dev.tx_ring = (e1000_tx_desc *)(ring_phys + hhdm_offset);
    memset(dev.tx_ring, 0, NUM_TX_DESC * sizeof(e1000_tx_desc));

    e1000_write(E1000_REG_TDBAL, (uint32_t)(ring_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_TDBAH, (uint32_t)(ring_phys >> 32));
    e1000_write(E1000_REG_TDLEN, NUM_TX_DESC * sizeof(e1000_tx_desc));
    
    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP;
    e1000_write(E1000_REG_TCTL, tctl);
}

void e1000_init(void)
{

    pci_dev = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID);
    
    if(!pci_dev) { 
        return; 
    }
    
    pci_enable_memory_space(pci_dev);
    pci_enable_busmaster(pci_dev);

    dev.mem_base = (uint64_t)(pci_dev->bar[0] & ~0xFULL) + hhdm_offset;
    e1000_reset();
    dev.has_eeprom = e1000_detect_eeprom();
    e1000_read_mac();
    e1000_init_rx();
    e1000_init_tx();

    debugln("E1000: Init complete. MAC: %02x:%02x:%02x:%02x:%02x:%02x", 1, 0,
	    dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]);
}

int e1000_send_packet(void *data, size_t len)
{
    uint16_t idx = dev.tx_cur;
    
    memcpy((void *)(dev.tx_ring[idx].addr + hhdm_offset), data, len);
    
    dev.tx_ring[idx].length = (uint16_t)len;
    dev.tx_ring[idx].cmd = E1000_CMD_EOP | E1000_CMD_RS | E1000_CMD_IFCS;
    dev.tx_ring[idx].status = 0;
    
    dev.tx_cur = (dev.tx_cur + 1) % NUM_TX_DESC;
    
    e1000_write(E1000_REG_TDT, dev.tx_cur);
    
    return len;
}

int e1000_receive_packet(void *buf, size_t buf_size)
{
    if (!(dev.rx_ring[dev.rx_cur].status & 0x01)) {
        return -1; // No packet available
    }

    uint16_t len = dev.rx_ring[dev.rx_cur].length;
    
    memcpy(buf, (void *)(dev.rx_ring[dev.rx_cur].addr + hhdm_offset), (len < buf_size ? len : buf_size));
    
    dev.rx_ring[dev.rx_cur].status = 0;
    
    uint16_t old_rdt = dev.rx_cur;
    dev.rx_cur = (dev.rx_cur + 1) % NUM_RX_DESC;
    e1000_write(E1000_REG_RDT, old_rdt);
    
    return len;
}

void e1000_handle_interrupt(void)
{
    uint32_t status = e1000_get_interrupt_status();
    if (status & E1000_ICR_LSC) debugln("E1000: Link Status Change", 1, 0);
    if (status & (E1000_ICR_RXT0)) net_poll();
}

void e1000_get_mac_address(uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
        mac[i] = dev.mac[i];
}

uint32_t e1000_link_up(void)
{
    return (e1000_read(E1000_REG_STATUS) & (1 << 1));
}

uint32_t e1000_get_interrupt_status(void)
{
    return e1000_read(E1000_REG_ICR);
}

void e1000_enable_interrupts(void)
{
    e1000_write(E1000_REG_IMS, 0x1F6DC); // Enable common interrupts
}

void e1000_disable_interrupts(void)
{
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF); // Disable all
}
