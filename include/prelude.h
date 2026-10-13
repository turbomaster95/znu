#ifndef _PRELUDE_H
#define _PRELUDE_H

#include <stdint.h>
#include <stddef.h>

#define PERFORM __attribute__((no_sanitize("undefined", "address", "thread"), noinline))

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#include <arch/prelude.h>

extern uint8_t* framebuffer_addr;
extern size_t framebuffer_size;

extern uint64_t framebuffer_width;
extern uint64_t framebuffer_height;
extern uint64_t framebuffer_pitch;
extern uint16_t framebuffer_bpp;

#endif
