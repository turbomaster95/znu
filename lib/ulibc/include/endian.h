#ifndef _ENDIAN_H
#define _ENDIAN_H

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN    4321
#define PDP_ENDIAN    3412

#define BYTE_ORDER LITTLE_ENDIAN

#if defined(__GNUC__) || defined(__clang__)
    #define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
    #define __BIG_ENDIAN    __ORDER_BIG_ENDIAN__
    #define __BYTE_ORDER    __BYTE_ORDER__
#else
    #define __LITTLE_ENDIAN LITTLE_ENDIAN
    #define __BIG_ENDIAN    BIG_ENDIAN
    #define __BYTE_ORDER    BYTE_ORDER
#endif

#define htobe16(x) __builtin_bswap16(x)
#define htole16(x) (x)
#define be16toh(x) __builtin_bswap16(x)
#define le16toh(x) (x)

#define htobe32(x) __builtin_bswap32(x)
#define htole32(x) (x)
#define be32toh(x) __builtin_bswap32(x)
#define le32toh(x) (x)

#define htobe64(x) __builtin_bswap64(x)
#define htole64(x) (x)
#define be64toh(x) __builtin_bswap64(x)
#define le64toh(x) (x)

#endif // _ENDIAN_H
