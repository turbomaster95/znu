#ifndef _STDIO_H
#define _STDIO_H 1

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define BUFSIZ 1024
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define ROUND_DOWN(v, n) ((v) - ((v) % (n)))
#define ROUND_UP(v, n) ROUND_DOWN((v) + (n) - 1, n)
#define FILENAME_MAX 4096

typedef struct {
    int fd;
} FILE;

extern FILE _stdin;
extern FILE _stdout;
extern FILE _stderr;

#define stdin  (&_stdin)
#define stdout (&_stdout)
#define stderr (&_stderr)

int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list ap);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int vsprintf(char* str, const char* format, va_list ap);

char* fgets(char* str, int n, FILE* stream);
void readline(char* buf, size_t n);
int putchar(int c);
int fputc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
int fflush(FILE *stream);
int fgetc(FILE *stream);
int fclose(FILE *stream);
int ferror(FILE *stream);
int fileno(FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *fopen(const char *filename, const char *mode);
void perror(const char *s);
int sscanf(const char *str, const char *format, ...);
int putc(int c, FILE *stream);
int ungetc(int c, FILE *stream);
int getc(FILE *stream);
int feof(FILE *stream);
int puts(const char *s);
int remove(const char *pathname);
int getc_unlocked(FILE *stream);
int getchar_unlocked(void);

#endif
