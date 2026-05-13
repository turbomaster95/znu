#include <string.h>

char *strcpy(char *dest, const char *src) {
    stac();
    char *d = dest;
    while ((*d++ = *src++));
    clac();
    return dest;
}

char *strcat(char *dest, const char *src) {
    stac();
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    clac();
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    stac();
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    clac();
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    stac();
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    clac();
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char* strrchr(const char* s, int c) {
    stac();
    char* last = NULL;
    char target = (char)c;

    do {
        if (*s == target) {
            last = (char*)s;
        }
    } while (*s++);
    clac();
    return last;
}
