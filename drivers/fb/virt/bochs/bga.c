#include <pci.h>
#include <stdio.h>
#include <stdlib.h>
#include <page.h>
#include <kernel/display.h>

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

bool using_bga = false;

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
    
    bga_write(BGA_IDX_VIRT_WIDTH, width);
    bga_write(BGA_IDX_X_OFFSET, 0);
    bga_write(BGA_IDX_Y_OFFSET, 0);
    
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

    using_bga = true;
    
    uint32_t width = 1024;
    uint32_t height = 768;
    uint32_t bpp = 32;
    
    bga_set_video_mode(width, height, bpp);
    
    // Clear the screen to black
    for (uint32_t i = 0; i < width * height; i++) {
        bga_framebuffer[i] = 0x00000000; // Deep Black
    }
    debugln("bga fb: %lx", bga_framebuffer);
    
    terminal_initialize_raw(
        (void*)bga_framebuffer, // Virtual address of the BGA LFB
        width,                  // 1024
        height,                 // 768
        width * 4,              // Pitch (bytes per scanline: 1024 * 4)
        8,                      // Red size
        16,                     // Red shift
        8,                      // Green size
        8,                      // Green shift
        8,                      // Blue size
        0                       // Blue shift
    );
}

void bga_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < current_width && y < current_height) {
        bga_framebuffer[y * current_width + x] = color;
    }
}

uint32_t* bga_get_lfb(void) {
    return bga_framebuffer;
}

void totally_normal_test(void) {
    uint32_t rainbow_colors[6] = {
        0x00E40303, // Red
        0x00FF8C00, // Orange
        0x00FFED00, // Yellow
        0x00008026, // Green
        0x0024408E, // Blue
        0x00732982  // Purple
    };

    uint32_t gray_colors[6] = {
        0x00000000, // Pure Black
        0x00333333, // Dark Gray
        0x00666666, // Medium Dark Gray
        0x00999999, // Medium Light Gray
        0x00CCCCCC, // Light Gray
        0x00FFFFFF  // Pure White
    };

    uint32_t stripe_height = current_height / 6;
    
    uint32_t central_width = (current_width * 15) / 100;
    uint32_t rainbow_start = (current_width / 2) - (central_width / 2);
    uint32_t rainbow_end = rainbow_start + central_width;

    for (uint32_t y = 0; y < current_height; y++) {
        uint32_t stripe_index = y / stripe_height;
        if (stripe_index > 5) stripe_index = 5; // Guard boundary

        for (uint32_t x = 0; x < current_width; x++) {
            if (x >= rainbow_start && x < rainbow_end) {
                bga_put_pixel(x, y, rainbow_colors[stripe_index]);
            } else {
                // Otherwise, draw the matching background grayscale block
                bga_put_pixel(x, y, gray_colors[stripe_index]);
            }
        }
    }
}
