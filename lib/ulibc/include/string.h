#ifndef _STRING_H
#define _STRING_H 1

#include <stddef.h>

size_t strlen(const char*);
char  *strcpy(char *dest, const char *src);
char  *strcat(char *dest, const char *src);
int    strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
char* strrchr(const char* s, int c);

#endif
