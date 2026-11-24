#include <stdlib.h>

int atoi(const char *nptr) {
    return (int)strtoll(nptr, NULL, 10);
}

long long strtoll(const char *nptr, char **endptr, int base) {
    long long res = 0;
    int sign = 1;
    while (*nptr == ' ') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;

    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return res * sign;
}
