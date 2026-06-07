#ifndef _STRINGS_H_
#define _STRINGS_H_

#include <stddef.h>

#define ffs(x)  __builtin_ffs(x)
#define ffsl(x) __builtin_ffsl(x)

int bcmp(const void *s1, const void *s2, size_t n);
void bcopy(const void *src, void *dest, size_t n);
void bzero(void *s, size_t n);

#endif /* !_STRINGS_H_ */
