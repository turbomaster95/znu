#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <kernel/tty.h>
#include <kernel/font8x8.h>

#if defined(__is_libk)
// Link to the request defined in your main.c
extern volatile struct limine_framebuffer_request framebuffer_request;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t color_fg = 0xFFFFFF; // White

// Minimal 8x8 Font Data for ASCII 32-126
// Each byte is a row (8 bits). 

void terminal_initialize(void) {
    cursor_x = 0;
    cursor_y = 0;
}

void set_cursor(uint32_t x, uint32_t y) {
    cursor_x = x;
    cursor_y = y;
}



void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    volatile uint32_t *fb_ptr = fb->address;

    // Use a pointer to the 8-byte sequence for this character
    char *glyph = font8x8_basic[(uint8_t)c];

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            // Check if the bit at 'col' is set in 'row'
            // We shift 1 by 'col' to create a mask
            if ((glyph[row] >> col) & 1) {
                // Drawing at 2x scale so it's readable on high-res screens
                for (int py = 0; py < 2; py++) {
                    for (int px = 0; px < 2; px++) {
                        uint64_t pix_x = x + (col * 2) + px;
                        uint64_t pix_y = y + (row * 2) + py;
                        fb_ptr[pix_y * (fb->pitch / 4) + pix_x] = fg;
                    }
                }
            }
        }
    }
}

void terminal_putchar(char c) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    if (c == '\n') {
        cursor_x = 0;
        cursor_y += 16; // 8 pixels * 2 scale
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        draw_char(c, cursor_x, cursor_y, 0xFFFFFF); // White text
        cursor_x += 16;
    }

    // Simple Screen Wrap
    if (cursor_x + 16 > fb->width) {
        cursor_x = 0;
        cursor_y += 16;
    }
    // TODO: Add scrolling logic here when cursor_y > fb->height!
}


void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    while (*data) terminal_putchar(*data++);
}

#endif
