#ifndef _CTYPE_H
#define _CTYPE_H

static inline int _isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

static inline int _isdigit(int c) {
    return (c >= '0' && c <= '9');
}

static inline int _isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static inline int _isalnum(int c) {
    return (_isalpha(c) || _isdigit(c));
}

static inline int _isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

static inline int _islower(int c) {
    return (c >= 'a' && c <= 'z');
}

static inline int _isxdigit(int c) {
    return (_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static inline int _iscntrl(int c) {
    return (c >= 0 && c < 32) || (c == 127);
}

static inline int _isprint(int c) {
    return (c >= 32 && c < 127);
}

static inline int _isgraph(int c) {
    return (c > 32 && c < 127);
}

static inline int _ispunct(int c) {
    return (_isgraph(c) && !_isalnum(c));
}

#ifndef isspace
#define isspace(c) _isspace(c)
#endif
#ifndef isdigit
#define isdigit(c) _isdigit(c)
#endif
#ifndef isalpha
#define isalpha(c) _isalpha(c)
#endif
#ifndef isalnum
#define isalnum(c) _isalnum(c)
#endif
#ifndef isupper
#define isupper(c) _isupper(c)
#endif
#ifndef islower
#define islower(c) _islower(c)
#endif
#ifndef isxdigit
#define isxdigit(c) _isxdigit(c)
#endif
#ifndef iscntrl
#define iscntrl(c) _iscntrl(c)
#endif
#ifndef isprint
#define isprint(c) _isprint(c)
#endif
#ifndef isgraph
#define isgraph(c) _isgraph(c)
#endif
#ifndef ispunct
#define ispunct(c) _ispunct(c)
#endif

static inline int toupper(int c) {
    if (_islower(c)) return c - 'a' + 'A';
    return c;
}

static inline int tolower(int c) {
    if (_isupper(c)) return c - 'A' + 'a';
    return c;
}

#endif
