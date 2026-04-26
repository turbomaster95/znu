#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <kernel/tty.h>
#include <kernel/font8x8.h>
#include <flanterm.h>
#include <flanterm_backends/fb.h>

// Declare the context so it's visible to all functions in this file
struct flanterm_context *ft_ctx = NULL;

#if defined(__is_libk)
// Link to the request defined in your main.c
extern volatile struct limine_framebuffer_request framebuffer_request;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t color_fg = 0xFFFFFF; // White

// Minimal 8x8 Font Data for ASCII 32-126
// Each byte is a row (8 bits). 

void terminal_initialize(void) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    ft_ctx = flanterm_fb_init(
        NULL, NULL,         // malloc, free (NULL uses default if available, or skip)
        fb->address,        // Framebuffer address
        fb->width,          // Width
        fb->height,         // Height
        fb->pitch,          // Pitch (bytes per line)
        fb->red_mask_size,  // Red mask size
        fb->red_mask_shift, // Red mask shift
        fb->green_mask_size,// Green mask size
        fb->green_mask_shift,// Green mask shift
        fb->blue_mask_size, // Blue mask size
        fb->blue_mask_shift,// Blue mask shift
        NULL,               // Canvas (NULL for default)
        NULL,               // ANSI colors
        NULL,               // Default ANSI colors
        NULL,               // Selection background
        NULL,               // Selection foreground
        NULL,               // Background image
        0,                  // Background style
        0,                  // Background opacity
        0,                  // Foreground opacity
        0,               // Font (NULL for built-in)
        0,                  // Font UI width
        0,                  // Font UI height
        0,                  // Font spacing
        0,                  // Margin
        0                   // Flags
    );
}

void terminal_putchar(char c) {
    if (!ft_ctx) return;

    if (c == '\n') {
        char cr = '\r';
        flanterm_write(ft_ctx, &cr, 1);
    }

    flanterm_write(ft_ctx, &c, 1);
}


void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    while (*data) terminal_putchar(*data++);
}

#endif
