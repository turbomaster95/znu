#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <syscall.h>

FILE _stdin  = { .fd = 0 };
FILE _stdout = { .fd = 1 };
FILE _stderr = { .fd = 2 };

int fflush(FILE *stream) {
    return 0; // Success, nothing to flush in a simple kernel console
}

int fgetc(FILE *stream) {
    // You'll eventually want to call your read() syscall here
    unsigned char c;
    if (read(stream->fd, &c, 1) <= 0) return EOF;
    return c;
}

FILE *fopen(const char *filename, const char *mode) {
    return NULL; // Return failure until you have a VFS
}

int fclose(FILE *stream) {
    return 0;
}

int ferror(FILE *stream) {
    return 0;
}

int fileno(FILE *stream) {
    if (!stream)
        return -1;

    return stream->fd;
}

int fputc(int c, FILE *stream) {
    char ch = (char)c;
    if (sys_write(stream->fd, &ch, 1) <= 0) return EOF;
    return c;
}

int sscanf(const char *str, const char *format, ...) {
    // If linenoise is looking for the cursor position: "\x1b[%d;%dR"
    if (str[0] == '\x1b' && str[1] == '[') {
        va_list args;
        va_start(args, format);
        int *row = va_arg(args, int *);
        int *col = va_arg(args, int *);
        
        // Simple manual parsing of "2;1R"
        int r = 0, c = 0;
        const char *p = str + 2;
        while(*p >= '0' && *p <= '9') r = r * 10 + (*p++ - '0');
        if (*p == ';') p++;
        while(*p >= '0' && *p <= '9') c = c * 10 + (*p++ - '0');
        
        *row = r;
        *col = c;
        va_end(args);
        return 2; // Found 2 items
    }
    return 0;
}
