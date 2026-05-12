#ifndef ZLINE_H
#define ZLINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// This is a small gnu readline typa thingy i made for znu
// You can use this in your kernel or embedded projects if you want.
// Just follow the License terms and credit this project in your code :) 

#ifdef ZL_FREESTANDING
    typedef long long ssize_t; 

    extern void* malloc(size_t size);
    extern void  free(void* ptr);
    extern ssize_t write(int fd, const void* buf, size_t n);
    extern ssize_t read(int fd, void* buf, size_t n);

    static size_t zl_strlen(const char *s) {
        size_t n = 0; while (s[n]) n++; return n;
    }

    static char *zl_strncpy(char *d, const char *s, size_t n) {
        size_t i;
        for (i = 0; i < n && s[i] != '\0'; i++) d[i] = s[i];
        for ( ; i < n; i++) d[i] = '\0';
        return d;
    }

    static int zl_strcmp(const char *s1, const char *s2) {
        while (*s1 && (*s1 == *s2)) { s1++; s2++; }
        return *(const unsigned char*)s1 - *(const unsigned char*)s2;
    }

    static void *zl_memset(void *s, int c, size_t n) {
        unsigned char *p = s; while (n--) *p++ = (unsigned char)c; return s;
    }

    static void *zl_memmove(void *d, const void *s, size_t n) {
        unsigned char *dest = d; const unsigned char *src = s;
        if (dest < src) { while (n--) *dest++ = *src++; }
        else { dest += n; src += n; while (n--) *--dest = *--src; }
        return d;
    }

    static char *zl_strstr(const char *h, const char *n) {
        if (!*n) return (char *)h;
        for (; *h; h++) {
            if (*h == *n) {
                const char *hh = h, *nn = n;
                while (*hh && *nn && *hh == *nn) { hh++; nn++; }
                if (!*nn) return (char *)h;
            }
        }
        return NULL;
    }

    #define ZL_STRLEN  zl_strlen
    #define ZL_STRNCPY zl_strncpy
    #define ZL_STRCMP  zl_strcmp
    #define ZL_MEMSET  zl_memset
    #define ZL_MEMMOVE zl_memmove
    #define ZL_STRSTR  zl_strstr
#else
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    #include <stdio.h>
    #define ZL_MALLOC  malloc
    #define ZL_FREE    free
    #define ZL_WRITE   write
    #define ZL_READ    read

    #define ZL_STRLEN  strlen
    #define ZL_STRNCPY strncpy
    #define ZL_STRCMP  strcmp
    #define ZL_MEMSET  memset
    #define ZL_MEMMOVE memmove
    #define ZL_STRSTR  strstr
#endif

#define ZL_MAX_BUF 4096
#define ZL_MAX_HIST 512

typedef struct {
    char **suggestions;
    size_t count;
} zl_completions_t;

typedef void(zl_completion_callback)(const char *buf, zl_completions_t *lc);

typedef struct {
    char *buf;
    char *temp_buf;
    size_t len, pos, last_render_len;
    char prompt[128];
    char *history[ZL_MAX_HIST];
    int hist_count, hist_idx;
    bool search_mode;
    char search_buf[64];
    char *(*complete_cb)(const char *input, int index);
    zl_completion_callback *completion_cb;
} zline_t;


static char *zl_strncat(char *dest, const char *src, size_t n) {
    size_t dest_len = ZL_STRLEN(dest);
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[dest_len + i] = src[i];
    dest[dest_len + i] = '\0';
    return dest;
}

static char *zl_strdup(const char *s) {
    size_t len = ZL_STRLEN(s);
    char *d = (char *)ZL_MALLOC(len + 1);
    if (!d) return NULL;
    for (size_t i = 0; i <= len; i++) d[i] = s[i];
    return d;
}

static void zl_puts(const char *s) {
    ZL_WRITE(1, s, ZL_STRLEN(s));
}

static void zl_refresh(zline_t *z) {
    zl_puts("\r\033[K");
    
    if (z->search_mode) {
        zl_puts("(reverse-i-search)`");
        zl_puts(z->search_buf);
        zl_puts("': ");
    } else {
        zl_puts(z->prompt);
    }
    
    zl_puts(z->buf);

    if (z->pos < z->len) {
        size_t diff = z->len - z->pos;
#ifdef ZL_FREESTANDING
        for (size_t i = 0; i < diff; i++) zl_puts("\033[D"); 
#else
        char seq[32];
        sprintf(seq, "\033[%zuD", diff);
        zl_puts(seq);
#endif
    }
}

static size_t zl_common_prefix(zl_completions_t *lc) {
    if (lc->count == 0) return 0;
    size_t prefix_len = 0;
    while (1) {
        char c = lc->suggestions[0][prefix_len];
        if (c == '\0') return prefix_len;
        for (size_t i = 1; i < lc->count; i++) {
            if (lc->suggestions[i][prefix_len] != c) return prefix_len;
        }
        prefix_len++;
    }
}

static int zl_startswith(const char *buf, const char *prefix) {
    while (*buf && *prefix) {
        if (*buf != *prefix) return 0;
        buf++;
        prefix++;
    }
    return *prefix == '\0';
}

static void zl_add_completion(zl_completions_t *lc, const char *str) {
    if (!str) return; // Safety check
    
    size_t len = ZL_STRLEN(str);
    char *copy = (char*)ZL_MALLOC(len + 1);
    if (!copy) return;
    ZL_STRNCPY(copy, str, len + 1);
    
    // Manual realloc replacement
    char **new_suggestions = (char**)ZL_MALLOC(sizeof(char*) * (lc->count + 1));
    if (!new_suggestions) {
        ZL_FREE(copy);
        return;
    }

    // Copy old pointers over
    for (size_t i = 0; i < lc->count; i++) {
        new_suggestions[i] = lc->suggestions[i];
    }
    
    if (lc->suggestions) ZL_FREE(lc->suggestions);
    
    lc->suggestions = new_suggestions;
    lc->suggestions[lc->count++] = copy;
}

static void zline_set_completion_callback(zline_t *z, zl_completion_callback *fn) {
    z->completion_cb = fn;
}

static void zl_history_add(zline_t *z, const char *line) {
    if (!line || !*line) return;
    if (z->hist_count > 0 && ZL_STRCMP(z->history[z->hist_count-1], line) == 0) return;

    if (z->hist_count == ZL_MAX_HIST) {
        ZL_FREE(z->history[0]);
        for (int i = 1; i < ZL_MAX_HIST; i++) z->history[i-1] = z->history[i];
        z->hist_count--;
    }
    z->history[z->hist_count++] = zl_strdup(line);
    z->hist_idx = z->hist_count;
}

static void zl_history_move(zline_t *z, int dir) {
    if (z->hist_count == 0) return;
    if (z->hist_idx == z->hist_count && dir == -1) {
        ZL_STRNCPY(z->temp_buf, z->buf, ZL_MAX_BUF - 1);
    }
    int new_idx = z->hist_idx + dir;
    if (new_idx < 0 || new_idx > z->hist_count) return;
    z->hist_idx = new_idx;
    if (new_idx == z->hist_count) {
        ZL_STRNCPY(z->buf, z->temp_buf, ZL_MAX_BUF - 1);
    } else {
        ZL_STRNCPY(z->buf, z->history[new_idx], ZL_MAX_BUF - 1);
    }
    z->len = z->pos = ZL_STRLEN(z->buf);
    zl_refresh(z);
}

static void zl_handle_search(zline_t *z, char c) {
    if (c == 127) {
        size_t slen = ZL_STRLEN(z->search_buf);
        if (slen > 0) z->search_buf[slen-1] = '\0';
    } else if (c >= 32 && c <= 126) {
        zl_strncat(z->search_buf, &c, 1);
    }

    for (int i = z->hist_count - 1; i >= 0; i--) {
        if (ZL_STRSTR(z->history[i], z->search_buf)) {
            ZL_STRNCPY(z->buf, z->history[i], ZL_MAX_BUF - 1);
            z->len = ZL_STRLEN(z->buf);
            z->pos = z->len;
            break;
        }
    }
    zl_refresh(z);
}

static char* zline_read(zline_t *z) {
    z->len = z->pos = 0;
    z->buf[0] = '\0';
    z->temp_buf[0] = '\0';
    z->search_mode = false;
    z->search_buf[0] = '\0';
    z->hist_idx = z->hist_count;
    zl_refresh(z);

    while (1) {
        char c;
        if (ZL_READ(0, &c, 1) <= 0) return NULL;
        if (z->search_mode) {
            if (c == 13 || c == 27) { z->search_mode = false; zl_refresh(z); if(c==27) continue; }
            else { zl_handle_search(z, c); continue; }
        }

        switch (c) {
            case 1:  z->pos = 0; break;
            case 5:  z->pos = z->len; break;
            case 3:  zl_puts("^C\n"); return NULL;
            case 13: zl_puts("\n"); zl_history_add(z, z->buf); return z->buf;
            case 18: z->search_mode = true; z->search_buf[0] = '\0'; break;
            case 127:
                if (z->pos > 0) {
                    ZL_MEMMOVE(&z->buf[z->pos-1], &z->buf[z->pos], z->len - z->pos + 1);
                    z->pos--; z->len--;
                }
                break;
            case 9: // TAB
                if (z->completion_cb) {
                    zl_completions_t lc = { .suggestions = NULL, .count = 0 };
                    z->completion_cb(z->buf, &lc);

                    if (lc.count > 0) {
                        size_t prefix_len = zl_common_prefix(&lc);
                        size_t current_len = ZL_STRLEN(z->buf);

                        // Only update if we found a prefix that ADDS information
                        if (prefix_len > current_len) {
                            ZL_STRNCPY(z->buf, lc.suggestions[0], prefix_len);
                            z->buf[prefix_len] = '\0';
                            z->len = z->pos = prefix_len;
                        }

                        if (lc.count > 1) {
                            // If there's more than one match, show the menu
                            zl_puts("\r\n");
                            for (size_t i = 0; i < lc.count; i++) {
                                zl_puts(lc.suggestions[i]);
                                zl_puts("  ");
                            }
                            zl_puts("\r\n");
                        }
                        zl_refresh(z);
                    }

                    // Cleanup
                    for (size_t i = 0; i < lc.count; i++) ZL_FREE(lc.suggestions[i]);
                    ZL_FREE(lc.suggestions);
                }
                break;
            case 27: {
                char seq[3];
                if (ZL_READ(0, &seq[0], 1) > 0 && ZL_READ(0, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        if (seq[1] == 'A') zl_history_move(z, -1);
                        if (seq[1] == 'B') zl_history_move(z, 1);
                        if (seq[1] == 'C' && z->pos < z->len) z->pos++;
                        if (seq[1] == 'D' && z->pos > 0) z->pos--;
                    }
                }
                break;
            }
            default:
                if (c >= 32 && z->len < ZL_MAX_BUF - 1) {
                    ZL_MEMMOVE(&z->buf[z->pos+1], &z->buf[z->pos], z->len - z->pos + 1);
                    z->buf[z->pos++] = c;
                    z->len++;
                }
                break;
        }
        zl_refresh(z);
    }
}

static zline_t* zline_init(const char *prompt) {
    zline_t *z = (zline_t*)ZL_MALLOC(sizeof(zline_t));
    ZL_MEMSET(z, 0, sizeof(zline_t));
    z->buf = (char*)ZL_MALLOC(ZL_MAX_BUF);
    z->temp_buf = (char*)ZL_MALLOC(ZL_MAX_BUF);
    ZL_STRNCPY(z->prompt, prompt, 127);
    return z;
}

#endif
