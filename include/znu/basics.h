#ifndef _BASICS_H
#define _BASICS_H

#include <stddef.h>

#define CONCAT_IMPL(a, b)   a##b
#define CONCAT(a, b)        CONCAT_IMPL(a, b)

#define STR_IMPL(x)         #x
#define STR(x)              STR_IMPL(x)

// Generates a unique identifier per line
#define UNIQUE_ID(prefix)   CONCAT(prefix, __LINE__)

#define MIN(a, b)           (((a) < (b)) ? (a) : (b))
#define MAX(a, b)           (((a) > (b)) ? (a) : (b))
#define CLAMP(x, lo, hi)    MIN(MAX((x), (lo)), (hi))

#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))

#define BIT(n)              (1ULL << (n))
#define BIT_SET(v, n)       ((v) |= BIT(n))
#define BIT_CLEAR(v, n)     ((v) &= ~BIT(n))
#define BIT_TOGGLE(v, n)    ((v) ^= BIT(n))
#define BIT_CHECK(v, n)     (!!((v) & BIT(n)))

#define ALIGN_UP(x, a)      (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)    (((x) & ((a) - 1)) == 0)

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))

// Recover parent struct pointer from a member pointer
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define STATIC_ASSERT(cond, msg) \
    typedef char UNIQUE_ID(_static_assert_##msg##_)[(cond) ? 1 : -1]

#define PACKED              __attribute__((packed))
#define ALIGNED(n)          __attribute__((aligned(n)))
#define SECTION(s)          __attribute__((section(s)))
#define UNUSED              __attribute__((unused))
#define USED                __attribute__((used))
#define NORETURN            __attribute__((noreturn))
#define WEAK                __attribute__((weak))
#define ALWAYS_INLINE       static inline __attribute__((always_inline))
#define NOINLINE            __attribute__((noinline))

#define likely(x)           __builtin_expect(!!(x), 1)
#define unlikely(x)         __builtin_expect(!!(x), 0)

#define barrier()           __asm__ __volatile__("" : : : "memory")

#define UNREACHABLE()       __builtin_unreachable()
#define UNUSED_VAR(x)       ((void)(x))

#endif /* _BASICS_H */
