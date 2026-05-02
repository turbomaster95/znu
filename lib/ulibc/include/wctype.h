#ifndef _WCTYPE_H
#define _WCTYPE_H

#include <wchar.h>

typedef int wctype_t;
typedef int wint_t;

int iswspace(wint_t wc);
int iswblank(wint_t wc);
int iswctype(wint_t wc, wctype_t desc);
wctype_t wctype(const char *name);

#endif
