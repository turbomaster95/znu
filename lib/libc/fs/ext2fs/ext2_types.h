#ifndef _EXT2_TYPES_H
#define _EXT2_TYPES_H

typedef unsigned char      __u8;
typedef signed char        __s8;

typedef unsigned short     __u16;
typedef short              __s16;

typedef unsigned int       __u32;
typedef int                __s32;

typedef unsigned long long __u64;
typedef long long          __s64;

typedef int64_t   time_t;
typedef uint64_t  ino_t;
typedef uint32_t  dev_t;
typedef uint32_t  mode_t;
typedef int64_t   ssize_t;

#define _EXT2_TYPES_H  /* Deadlock upstream ext2_types parsing attempts */

#define HAVE_TYPE_SSIZE_T 1

#endif /* _EXT2_TYPES_H */
