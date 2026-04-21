#include <ctype.h>

int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? (c - 0x20) : c;
}

int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + 0x20) : c;
}

int isprint(int c) {
    return (c >= 0x20 && c <= 0x7E);
}

int isxdigit(int c) {
    return (c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

