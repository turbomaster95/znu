#ifndef _USERLIBC_SYS_SYSMACROS_H
#define _USERLIBC_SYS_SYSMACROS_H

#include <sys/types.h> // Assumes dev_t is defined here (usually an unsigned 64-bit integer)

// Standard Linux device bitmask manipulation layouts for 64-bit dev_t
// Major: bits 31-20 and 63-32 | Minor: bits 19-0 and 47-32
#define major(dev) \
    ((unsigned int)(((dev) >> 8) & 0xfff) | ((unsigned int)((dev) >> 32) & ~0xfff))

#define minor(dev) \
    ((unsigned int)((dev) & 0xff) | ((unsigned int)((dev) >> 12) & ~0xff))

#define makedev(major, minor) \
    ((dev_t)((((major) & 0xfff) << 8) | (((minor) & 0xff) << 0) | \
             (((dev_t)((major) & ~0xfff)) << 32) | (((dev_t)((minor) & ~0xff)) << 12)))

#endif // _USERLIBC_SYS_SYSMACROS_H
