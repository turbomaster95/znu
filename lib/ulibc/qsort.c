#include <stdlib.h>

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    char *b = base;
    for (size_t i = 0; i < nmemb; i++) {
        for (size_t j = i + 1; j < nmemb; j++) {
            if (compar(b + i * size, b + j * size) > 0) {
                for (size_t k = 0; k < size; k++) {
                    char tmp = b[i * size + k];
                    b[i * size + k] = b[j * size + k];
                    b[j * size + k] = tmp;
                }
            }
        }
    }
}
