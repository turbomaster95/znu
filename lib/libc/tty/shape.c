#include <stdint.h>
#include <limine.h>

static inline int abs(int j) {
    return j < 0 ? -j : j;
}

extern volatile struct limine_framebuffer_request framebuffer_request;

static inline void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    // Safety check to prevent out-of-bounds memory writes (Kernel Panic territory!)
    if (x >= fb->width || y >= fb->height) return;

    volatile uint32_t *fb_ptr = fb->address;
    // Math: Y * (Width in pixels) + X
    // We use pitch / 4 because pitch is in bytes, and each pixel is 4 bytes (32-bit)
    fb_ptr[y * (fb->pitch / 4) + x] = color;
}

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            put_pixel(x + j, y + i, color);
        }
    }
}

void draw_outline_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < w; i++) {
        put_pixel(x + i, y, color);         // Top
        put_pixel(x + i, y + h - 1, color); // Bottom
    }
    for (uint32_t i = 0; i < h; i++) {
        put_pixel(x, y + i, color);         // Left
        put_pixel(x + w - 1, y + i, color); // Right
    }
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_circle(int xm, int ym, int r, uint32_t color) {
    int x = -r, y = 0, err = 2 - 2 * r;
    do {
        put_pixel(xm - x, ym + y, color);
        put_pixel(xm - y, ym - x, color);
        put_pixel(xm + x, ym - y, color);
        put_pixel(xm + y, ym + x, color);
        r = err;
        if (r <= y) err += ++y * 2 + 1;
        if (r > x || err > y) err += ++x * 2 + 1;
    } while (x < 0);
}
