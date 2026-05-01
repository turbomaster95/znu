#include <string.h>

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char* strrchr(const char* s, int c) {
    char* last = NULL;
    char target = (char)c;

    do {
        if (*s == target) {
            last = (char*)s;
        }
    } while (*s++);

    return last;
}

char *strtok(char *s, const char *delim) {
    static char *old;
    if (s) old = s;
    if (!old) return 0;
    
    // Skip leading delimiters
    while (*old && strchr(delim, *old)) old++;
    if (!*old) return 0;

    char *ret = old;
    // Find end of token
    while (*old && !strchr(delim, *old)) old++;
    if (*old) {
        *old = '\0';
        old++;
    } else {
        old = 0;
    }
    return ret;
}

size_t strspn(const char *s, const char *accept) {
    const char *p = s;
    const char *a;
    size_t count = 0;
    for (; *p; p++) {
        for (a = accept; *a; a++) {
            if (*p == *a) break;
        }
        if (*a == '\0') return count;
        count++;
    }
    return count;
}

size_t strcspn(const char *s, const char *reject) {
    const char *p = s;
    const char *r;
    size_t count = 0;
    for (; *p; p++) {
        for (r = reject; *r; r++) {
            if (*p == *r) return count;
        }
        count++;
    }
    return count;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char *)s;
        s++;
    }
    return NULL;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}
