#ifndef __LWIP_ARCH_CC_H__
#define __LWIP_ARCH_CC_H__

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t  u8_t;
typedef int8_t   s8_t;
typedef uint16_t u16_t;
typedef int16_t  s16_t;
typedef uint32_t u32_t;
typedef int32_t  s32_t;

#define U8_F   "u"
#define S8_F   "d"
#define U16_F  "u"
#define S16_F  "d"
#define U32_F  "u"
#define S32_F  "d"

#define X8_F   "02x"
#define X16_F  "04x"
#define X32_F  "08x"

#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_USE_INCLUDES

#define LWIP_PLATFORM_ASSERT(x) do {p_error("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); abort();} while(0)

u32_t lwip_rand(void);
#define LWIP_RAND lwip_rand

#endif
