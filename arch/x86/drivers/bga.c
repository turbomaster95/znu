#include <pci.h>
#include <stdio.h>
#include <stdlib.h>
#include <page.h>

#define BGA_PORT_INDEX          0x01CE
#define BGA_PORT_VALUE          0x01CF

#define BGA_IDX_ID              0x00
#define BGA_IDX_XRES            0x01
#define BGA_IDX_YRES            0x02
#define BGA_IDX_BPP             0x03
#define BGA_IDX_ENABLE          0x04
#define BGA_IDX_BANK            0x05
#define BGA_IDX_VIRT_WIDTH      0x06
#define BGA_IDX_VIRT_HEIGHT     0x07
#define BGA_IDX_X_OFFSET        0x08
#define BGA_IDX_Y_OFFSET        0x09

#define BGA_DISABLED            0x00
#define BGA_ENABLED             0x01
#define BGA_LFB_ENABLED         0x40
#define BGA_NO_CLEAR_MEM        0x80

static uint32_t* bga_framebuffer = NULL;
static uint16_t current_width = 0;
static uint16_t current_height = 0;

static void bga_write(uint16_t index, uint16_t value) {
    outw(BGA_PORT_INDEX, index);
    outw(BGA_PORT_VALUE, value);
}

static uint16_t bga_read(uint16_t index) {
    outw(BGA_PORT_INDEX, index);
    return inw(BGA_PORT_VALUE);
}

void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    current_width = width;
    current_height = height;

    bga_write(BGA_IDX_ENABLE, BGA_DISABLED);
    bga_write(BGA_IDX_XRES, width);
    bga_write(BGA_IDX_YRES, height);
    bga_write(BGA_IDX_BPP, bpp);
    
    bga_write(BGA_IDX_ENABLE, BGA_ENABLED | BGA_LFB_ENABLED);
}

void bga_init(void) {
    // Vendor 0x1234, Device 0x1111 (Class 03, Subclass 00)
    pci_device_t* dev = pci_find_class(0x03, 0x00, 0x00);
    
    if (!dev || dev->vendor_id != 0x1234) {
        debugln("[bga] Device not found or incompatible.");
        return;
    }

    uintptr_t phys_fb = dev->bar[0] & 0xFFFFFFF0;
    bga_framebuffer = (uint32_t*)(phys_fb + hhdm_offset);

    uint16_t version = bga_read(BGA_IDX_ID);
    debugln("[bga] Initialized. ID: 0x%04X, LFB at: %p", version, bga_framebuffer);

    bga_set_video_mode(1024, 768, 32);
}

void bga_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < current_width && y < current_height) {
        bga_framebuffer[y * current_width + x] = color;
    }
}

uint32_t* bga_get_lfb(void) {
    return bga_framebuffer;
}
