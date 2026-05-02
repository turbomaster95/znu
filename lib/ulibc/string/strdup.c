#include <string.h>
#include <stdlib.h>

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *new_s = malloc(len);
    if (new_s == NULL) return NULL;
    return memcpy(new_s, s, len);
}

char *strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    char *new_s = malloc(len + 1);
    if (new_s == NULL) return NULL;
    new_s[len] = '\0';
    return memcpy(new_s, s, len);
}
