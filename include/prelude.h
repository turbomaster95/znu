#ifndef _PRELUDE_H
#define _PRELUDE_H

#ifdef __cplusplus
#define _Noreturn [[noreturn]]
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define PERFORM __attribute__((no_sanitize("undefined", "address", "thread"), noinline))

// Basic C preprocessor macros
#include <znu/basics.h>

#include <arch/prelude.h>

extern uint8_t* framebuffer_addr;
extern size_t framebuffer_size;

extern uint64_t framebuffer_width;
extern uint64_t framebuffer_height;
extern uint64_t framebuffer_pitch;
extern uint16_t framebuffer_bpp;

// Include kernel headers needed by everything
#include <kernel/logging.h>

#ifdef __cplusplus
}
#endif

#endif
