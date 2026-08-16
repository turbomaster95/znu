#ifndef _DRIVERS_BGA_H
#define _DRIVERS_BGA_H

#include <stdint.h>
#include <stdbool.h>

#if defined(CONFIG_BGA) || defined(CONFIG_BGA_MODULE) || defined(MODULE)

extern bool using_bga;

void bga_init(void);
void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp);
void bga_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t* bga_get_lfb(void);
void totally_normal_test(void);

#else

static inline void bga_init(void) {}
static inline void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    (void)width; (void)height; (void)bpp;
}
static inline void bga_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    (void)x; (void)y; (void)color;
}
static inline uint32_t* bga_get_lfb(void) { return (uint32_t*)0; }
static inline void totally_normal_test(void) {}

#define using_bga (false)

#endif /* CONFIG_BGA */

#endif /* _DRIVERS_BGA_H */
