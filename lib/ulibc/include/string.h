#ifndef _STRING_H
#define _STRING_H 1

#include <stddef.h>

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char*);
char  *strcpy(char *dest, const char *src);
char  *strcat(char *dest, const char *src);
int    strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
char* strrchr(const char* s, int c);
char *strtok(char *str, const char *delim);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strstr(const char *haystack, const char *needle);
char *strpbrk(const char *s, const char *accept);
int strcoll(const char *s1, const char *s2);
char *stpncpy(char *dest, const char *src, size_t n);
char *strsignal(int sig);
char *strerror(int errnum);

#endif
