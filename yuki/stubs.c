#include <stddef.h>

void exit(int code) {
    for (;;) {
        __asm__("hlt");
    }
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, void *stream) {
    return 0;
}

int fflush(void *stream) {
    return 0;
}

void *stderr = NULL;
