#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <string.h>
#include <page.h>
#include <kernel/display.h>
#include <flanterm.h>
#include <flanterm_backends/fb.h>

struct flanterm_context *ft_ctx = NULL;

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

void terminal_initialize_raw(
    void* fb_address, 
    uint64_t width, 
    uint64_t height, 
    uint64_t pitch,
    uint8_t red_mask_size,
    uint8_t red_mask_shift,
    uint8_t green_mask_size,
    uint8_t green_mask_shift,
    uint8_t blue_mask_size,
    uint8_t blue_mask_shift
) {
    ft_ctx = flanterm_fb_init(
        flanterm_malloc,
        flanterm_free,
        fb_address,
        width,               // BGA Width (e.g., 1024)
        height,              // BGA Height (e.g., 768)
        pitch,               // BGA Pitch (usually width * 4)
        red_mask_size,       
        red_mask_shift, 
        green_mask_size,
        green_mask_shift,
        blue_mask_size, 
        blue_mask_shift,
        NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0
    );
}

void terminal_initialize(void) {
    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }
    
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    
    terminal_initialize_raw(
        fb->address,
        fb->width,
        fb->height,
        fb->pitch,
        fb->red_mask_size,
        fb->red_mask_shift,
        fb->green_mask_size,
        fb->green_mask_shift,
        fb->blue_mask_size,
        fb->blue_mask_shift
    );
}

void terminal_putchar(char c) {
    if (!ft_ctx) return;
    flanterm_write(ft_ctx, &c, 1);
}

void terminal_backspace(void) {
    if (!ft_ctx)
        return;

    char bs = '\b';
    char sp = ' ';

    flanterm_write(ft_ctx, &bs, 1);
    flanterm_write(ft_ctx, &sp, 1);
    flanterm_write(ft_ctx, &bs, 1);
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    while (*data) terminal_putchar(*data++);
}

void terminal_teardown(void) {
	if (!ft_ctx) return;
	flanterm_deinit(ft_ctx, flanterm_free);
	flanterm_clear(ft_ctx, true);
	ft_ctx = NULL;
}

#endif
