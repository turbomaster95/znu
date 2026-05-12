#include <syscall.h>
#include <stdio.h>
#include <stdint.h>

int getchar(void) {
    char c;
    // sys_read returns immediately with available chars (usually 1)
    while (sys_read(0, &c, 1) == 0) {
        // Shouldn't happen with blocking read, but just in case
    }
    return c;
}

void readline(char* buf, size_t n) {
    size_t i = 0;
    while (i < n - 1) {
        char c = getchar();
        
        // Handle backspace (0x0E scancode → '\b' from keyboard table, or DEL 0x7F)
        if (c == '\b' || c == 0x7F) {
            if (i > 0) {
                i--;
                // Visual erase: move back, space over, move back again
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
            continue;
        }
        
        // Echo printable chars
        if (c >= 0x20 && c < 0x7F) {
           // putchar(c); // Kernel echoes for us
        } else if (c == '\n' || c == '\r') {
            // putchar('\n'); // Kernel echoes for us
            // buf[i++] = '\n';
            break;
        } else {
            // Control chars — don't echo or store
            continue;
        }
        
        buf[i++] = c;
    }
    buf[i] = '\0';
}

char* fgets(char* str, int n, FILE* stream) {
    int fd = stream->fd;
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
