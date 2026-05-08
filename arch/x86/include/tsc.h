#ifndef TSC_H
#define TSC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool supported;
    uint64_t frequency; // Hz
} tsc_info_t;

tsc_info_t tsc_detect(void);
uint64_t tsc_read(void);
uint64_t tsc_get_frequency(void);

#endif /* TSC_H */
