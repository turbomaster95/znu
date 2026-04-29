#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <string.h>
#include <page.h>
#include <kernel/tty.h>
#include <flanterm.h>
#include <flanterm_backends/fb.h>

struct flanterm_context *ft_ctx = NULL;
uint32_t TERM_W = 800;
uint32_t TERM_H = 600;
uint32_t term_x = 0;
uint32_t term_y = 0;

uint32_t *term_buffer = NULL;

#if defined(__is_libk)
extern volatile struct limine_framebuffer_request framebuffer_request;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t color_fg = 0xFFFFFF; // White

void* flanterm_malloc(size_t size) {
    return kmalloc(size);
}

void flanterm_free(void* ptr, size_t size) {
    (void)size; // Prevent unused parameter warning
    kfree(ptr);
}

void blit_window(int win_x, int win_y, int win_w, int win_h, uint32_t *win_buffer) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = fb->address;

    for (int i = 0; i < win_h; i++) {
        // Calculate the start of the row in the window and on the screen
        void *src = &win_buffer[i * win_w];
        void *dest = &fb_ptr[(win_y + i) * (fb->pitch / 4) + win_x];
        
        // Copy one horizontal line of pixels
        memcpy(dest, src, win_w * 4);
    }
}

void terminal_initialize(void) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    term_buffer = kmalloc(TERM_W * TERM_H * 4);
    
    // Safety check: ensure kmalloc didn't fail
    if (term_buffer == NULL) return;

    ft_ctx = flanterm_fb_init(
        flanterm_malloc,
        flanterm_free,
//        fb->address,        // Framebuffer address
	term_buffer,
//        fb->width,          // Width
	TERM_W,
//        fb->height,         // Height
	TERM_H,
//        fb->pitch,          // Pitch (bytes per line)
	TERM_W * 4,  
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
        0,                  // Font (NULL for built-in)
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
