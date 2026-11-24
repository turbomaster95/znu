#include <e1000.h>
#include <net.h>
#include <page.h>
#include <pci.h>
#include <lapic.h>
#include <idt.h>
#include <stdlib.h>

static e1000_device dev;
pci_device_t *e1000_pci_dev = NULL;
static void *rx_buf_virt[NUM_RX_DESC];
static void *tx_buf_virt[NUM_TX_DESC];

#if (PAGE_SIZE % RX_BUFFER_SIZE) != 0 || (PAGE_SIZE % TX_BUFFER_SIZE) != 0
#error "PAGE_SIZE must be a multiple of the E1000 buffer sizes"
#endif
#define RX_BUFS_PER_PAGE   (PAGE_SIZE / RX_BUFFER_SIZE)
#define TX_BUFS_PER_PAGE   (PAGE_SIZE / TX_BUFFER_SIZE)

#define e1000_barrier()    __sync_synchronize()

static uint32_t e1000_read(uint32_t reg)
{
    return *((volatile uint32_t *)(dev.mem_base + reg));
}

static void e1000_write(uint32_t reg, uint32_t value)
{
    *((volatile uint32_t *)(dev.mem_base + reg)) = value;
}

static uint32_t e1000_eeprom_read(uint8_t addr)
{
    uint32_t tmp = 0;

    if (dev.has_eeprom)
    {
        e1000_write(E1000_REG_EEPROM, E1000_EERD_START | ((uint32_t)addr << E1000_EERD_ADDR_SHIFT));
        for (int i = 0; i < 100000; i++)
        {
            tmp = e1000_read(E1000_REG_EEPROM);
            if (tmp & E1000_EERD_DONE)
                return (tmp & E1000_EERD_DATA_MASK) >> E1000_EERD_DATA_SHIFT;
        }
    }
    else
    {
        e1000_write(E1000_REG_EEPROM, E1000_EERD_START | ((uint32_t)addr << E1000_EERD_ADDR_SHIFT_ICH));
        for (int i = 0; i < 100000; i++)
        {
            tmp = e1000_read(E1000_REG_EEPROM);
            if (tmp & E1000_EERD_DONE_ICH)
                return (tmp & E1000_EERD_DATA_MASK) >> E1000_EERD_DATA_SHIFT;
        }
    }

    return 0xFFFF;
}

static uint8_t e1000_detect_eeprom(void)
{
    e1000_write(E1000_REG_EEPROM, E1000_EERD_START);
    for (int i = 0; i < 10000; i++)
    {
        uint32_t val = e1000_read(E1000_REG_EEPROM);
        if (val & E1000_EERD_DONE)
            return 1;
    }
    return 0;
}

static int e1000_reset(void)
{
    uint32_t ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);
    for (int i = 0; i < 100000; i++)
    {
        if (!(e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST))
        {
            for (volatile int i2 = 0; i2 < 1000; i2++)
                ;
            return 0;
        }
    }
    return -1;
}

static void e1000_set_mac_hw(void)
{
    uint32_t low  = ((uint32_t)dev.mac[0]) | ((uint32_t)dev.mac[1] << 8) |
                    ((uint32_t)dev.mac[2] << 16) | ((uint32_t)dev.mac[3] << 24);
    uint32_t high = ((uint32_t)dev.mac[4]) | ((uint32_t)dev.mac[5] << 8) |
                    E1000_RAH_AV;

    e1000_write(E1000_REG_RAL, low);
    e1000_write(E1000_REG_RAH, high);
    e1000_barrier();
}

static int mac_is_valid(const uint8_t *m)
{
    int all_zero = 1, all_ff = 1;
    for (int i = 0; i < 6; i++)
    {
        if (m[i] != 0x00) all_zero = 0;
        if (m[i] != 0xFF) all_ff = 0;
    }
    return !all_zero && !all_ff;
}

static void e1000_read_mac(void)
{
    uint32_t mac_low = e1000_read(E1000_REG_RAL);
    uint32_t mac_high = e1000_read(E1000_REG_RAH);

    dev.mac[0] = mac_low & 0xFF;
    dev.mac[1] = (mac_low >> 8) & 0xFF;
    dev.mac[2] = (mac_low >> 16) & 0xFF;
    dev.mac[3] = (mac_low >> 24) & 0xFF;
    dev.mac[4] = mac_high & 0xFF;
    dev.mac[5] = (mac_high >> 8) & 0xFF;

    if (!mac_is_valid(dev.mac) && dev.has_eeprom)
    {
        uint32_t temp = e1000_eeprom_read(0);
        dev.mac[0] = temp & 0xFF; dev.mac[1] = temp >> 8;
        temp = e1000_eeprom_read(1);
        dev.mac[2] = temp & 0xFF; dev.mac[3] = temp >> 8;
        temp = e1000_eeprom_read(2);
        dev.mac[4] = temp & 0xFF; dev.mac[5] = temp >> 8;
    }

    if (!mac_is_valid(dev.mac))
    {
        dev.mac[0] = 0x52; dev.mac[1] = 0x54;
        dev.mac[2] = 0x00; dev.mac[3] = 0x12;
        dev.mac[4] = 0x34; dev.mac[5] = 0x56;
    }

    e1000_set_mac_hw();

    for (int i = 0; i < 128; i++)
        e1000_write(E1000_REG_MTA + (uint32_t)i * 4, 0);
}

static int e1000_alloc_rx_buffers(void)
{
    for (int i = 0; i < NUM_RX_DESC; i += RX_BUFS_PER_PAGE)
    {
        uint64_t page = (uint64_t)palloc_zero();
        if (!page)
            return -1;

        for (int j = 0; j < RX_BUFS_PER_PAGE; j++)
        {
            int d = i + j;
            uint64_t buf_phys = page + (uint64_t)j * RX_BUFFER_SIZE;
            dev.rx_ring[d].addr = buf_phys;
            dev.rx_ring[d].status = 0;
            rx_buf_virt[d] = (void *)(buf_phys + hhdm_offset);
        }
    }
    return 0;
}

static int e1000_alloc_tx_buffers(void)
{
    for (int i = 0; i < NUM_TX_DESC; i += TX_BUFS_PER_PAGE)
    {
        uint64_t page = (uint64_t)palloc_zero();
        if (!page)
            return -1;

        for (int j = 0; j < TX_BUFS_PER_PAGE; j++)
        {
            int d = i + j;
            uint64_t buf_phys = page + (uint64_t)j * TX_BUFFER_SIZE;
            dev.tx_ring[d].addr = buf_phys;
            dev.tx_ring[d].status = 0;
            tx_buf_virt[d] = (void *)(buf_phys + hhdm_offset);
        }
    }
    return 0;
}

static int e1000_init_rx(void)
{
    uint64_t ring_phys = (uint64_t)palloc_zero();
    if (!ring_phys)
        return -1;

    dev.rx_ring = (e1000_rx_desc *)(ring_phys + hhdm_offset);

    if (e1000_alloc_rx_buffers() != 0)
        return -1;

    e1000_write(E1000_REG_RDBAL, (uint32_t)(ring_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_RDBAH, (uint32_t)(ring_phys >> 32));
    e1000_write(E1000_REG_RDLEN, NUM_RX_DESC * sizeof(e1000_rx_desc));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, NUM_RX_DESC - 1);
    e1000_barrier();

    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_UPE |
                    E1000_RCTL_MPE | E1000_RCTL_SZ_2048;
    e1000_write(E1000_REG_RCTL, rctl);
    return 0;
}

static int e1000_init_tx(void)
{
    uint64_t ring_phys = (uint64_t)palloc_zero();
    if (!ring_phys)
        return -1;

    dev.tx_ring = (e1000_tx_desc *)(ring_phys + hhdm_offset);

    if (e1000_alloc_tx_buffers() != 0)
        return -1;

    e1000_write(E1000_REG_TDBAL, (uint32_t)(ring_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_TDBAH, (uint32_t)(ring_phys >> 32));
    e1000_write(E1000_REG_TDLEN, NUM_TX_DESC * sizeof(e1000_tx_desc));
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);
    e1000_barrier();

    e1000_write(E1000_REG_TIPG, E1000_TIPG_DEFAULT);

    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP |
                    ((uint32_t)E1000_COLLISION_THRESHOLD << E1000_TCTL_CT_SHIFT) |
                    ((uint32_t)E1000_COLLISION_DISTANCE << E1000_TCTL_COLD_SHIFT);
    e1000_write(E1000_REG_TCTL, tctl);
    return 0;
}

int e1000_init(void)
{
    static const uint32_t ids[] = {
        E1000_DEVICE_ID,    /* 82540EM (QEMU) */
        E1000_DEVICE_ID_3,  /* 82577LM - legacy descriptor mode only */
        E1000_DEVICE_ID_2,  /* I219-V   - legacy descriptor mode only */
    };

    memset(&dev, 0, sizeof(dev));

    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
    {
        e1000_pci_dev = pci_find_device(E1000_VENDOR_ID, ids[i]);
        if (e1000_pci_dev)
        {
            dev.device_id = (uint16_t)ids[i];
            break;
        }
    }

    if (!e1000_pci_dev)
        return -1;

    pci_enable_memory_space(e1000_pci_dev);
    pci_enable_busmaster(e1000_pci_dev);

    if ((e1000_pci_dev->bar[0] & 0x1) != 0)
        return -1; /* IO-space BAR: unsupported on this part */

    dev.mem_base = (e1000_pci_dev->bar[0] & ~0xFULL) + hhdm_offset;

    if (e1000_reset() != 0)
        return -1;

    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU);

    dev.has_eeprom = e1000_detect_eeprom();
    e1000_read_mac();

    if (e1000_init_rx() != 0)
        return -1;
    if (e1000_init_tx() != 0)
        return -1;

    e1000_disable_interrupts();
    (void)e1000_read(E1000_REG_ICR); /* clear pending causes */

    dev.present = 1;

    p_info("e1000", "E1000: Init complete. MAC: %02x:%02x:%02x:%02x:%02x:%02x",
           dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]);

    return 0;
}

int e1000_present(void)
{
    return dev.present;
}

int e1000_send_packet(void *data, size_t len)
{
    if (!dev.present || len == 0 || len > TX_BUFFER_SIZE)
        return -1;

    while (dev.tx_in_flight > 0)
    {
        if (dev.tx_ring[dev.tx_dirty].status & E1000_TXD_STAT_DD)
        {
            dev.tx_dirty = (dev.tx_dirty + 1) % NUM_TX_DESC;
            dev.tx_in_flight--;
        }
        else
            break;
    }

    if (dev.tx_in_flight == NUM_TX_DESC)
        return -1; /* ring full: back-pressure to the caller */

    uint16_t idx = dev.tx_cur;

    memcpy(tx_buf_virt[idx], data, len);

    dev.tx_ring[idx].length = (uint16_t)len;
    dev.tx_ring[idx].cso = 0;
    dev.tx_ring[idx].cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS;
    dev.tx_ring[idx].status = 0;

    e1000_barrier();

    dev.tx_cur = (dev.tx_cur + 1) % NUM_TX_DESC;
    dev.tx_in_flight++;
    e1000_write(E1000_REG_TDT, dev.tx_cur);

    return (int)len;
}

int e1000_receive_packet(void *buf, size_t buf_size)
{
    if (!dev.present || !buf || buf_size == 0)
        return -1;

    static int discard_chain = 0;

    for (;;)
    {
        e1000_rx_desc *desc = &dev.rx_ring[dev.rx_cur];

        if (!(desc->status & E1000_RXD_STAT_DD))
            return -1; /* no packet (or chain piece) available */

        uint16_t len = desc->length;
        uint16_t next = (dev.rx_cur + 1) % NUM_RX_DESC;
        int deliver = 0;

        if (discard_chain)
        {
            if (desc->status & E1000_RXD_STAT_EOP)
                discard_chain = 0;
        }
        else if (desc->errors & E1000_RXD_ERR_RXE)
        {
        }
        else if (desc->status & E1000_RXD_STAT_EOP)
        {
            deliver = 1;
        }
        else
        {
            discard_chain = 1;
        }

        size_t copied = 0;
        if (deliver)
        {
            copied = (len < buf_size) ? len : buf_size;
            memcpy(buf, rx_buf_virt[dev.rx_cur], copied);
        }

        /* hand the descriptor back to the NIC */
        desc->errors = 0;
        desc->status = 0;
        e1000_barrier();
        e1000_write(E1000_REG_RDT, dev.rx_cur);

        dev.rx_cur = next;

        if (deliver)
            return (int)copied;
    }
}

void e1000_handle_interrupt(void)
{
    uint32_t status = e1000_read(E1000_REG_ICR);

    if (status & E1000_ICR_LSC)
        p_debug("e1000", "E1000: Link Status Change");

    if (status & (E1000_ICR_RXT0 | E1000_ICR_RXO))
        net_poll();
}

void e1000_get_mac_address(uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
        mac[i] = dev.mac[i];
}

uint32_t e1000_link_up(void)
{
    return (e1000_read(E1000_REG_STATUS) & E1000_STATUS_LU) ? 1 : 0;
}

uint32_t e1000_get_interrupt_status(void)
{
    return e1000_read(E1000_REG_ICR);
}

void e1000_enable_interrupts(void)
{
    e1000_write(E1000_REG_IMS, E1000_IMS_DEFAULT);
}

void e1000_disable_interrupts(void)
{
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF);
}
