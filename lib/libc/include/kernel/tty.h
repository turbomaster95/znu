#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>
#include <stdint.h>

// Some hacky helper functions to make coloring easier
// these can be called by anyone so no libk exclusivity
#define RGB(r, g, b) (uint32_t)(0xFF000000 | ((r) << 16) | ((g) << 8) | (b))
uint32_t color_hex(uint32_t hex);

extern uint32_t TERM_W;
extern uint32_t TERM_H;
extern uint32_t term_x;
extern uint32_t term_y;

#if defined(__is_libk)
extern uint32_t *term_buffer;

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_outline_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_circle(int xm, int ym, int r, uint32_t color);
void set_cursor(uint32_t x, uint32_t y);

#endif
void blit_window(int win_x, int win_y, int win_w, int win_h, uint32_t *win_buffer);

#endif
