#include <syscall.h>

int getchar(void) {
    char c;
    while (sys_read(0, &c, 1) == 0) {
        // Block until key available
        // TODO: yield or sleep instead of spin
    }
    return c;
}

char* fgets(char* str, int n, int fd) {
    int i = 0;
    while (i < n - 1) {
        char c;
        size_t r = sys_read(fd, &c, 1);
        if (r <= 0) {
            if (i == 0) return NULL;
            break;
        }
        str[i++] = c;
        if (c == '\n') break;
    }
    str[i] = '\0';
    return str;
}
