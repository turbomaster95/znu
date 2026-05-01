#ifndef _WCHAR_H
#define _WCHAR_H

#include <stddef.h>

typedef int mbstate_t;
typedef int wchar_t;

size_t mbrlen(const char *s, size_t n, mbstate_t *ps);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps);
wchar_t *wcschr(const wchar_t *s, wchar_t c);

#endif
