#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <stdint.h>
#include <uacpi/uacpi.h>
#include <generated/nifties.h>

void timekeeper_on_tick(void);
void timekeeper_init(void);
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void);
uint64_t timekeeper_timefromboot(void);

// Nifties -> Milliseconds
static inline uint64_t nifties_to_msecs(uint64_t nifties) {
    return (nifties * 1000ULL) / CONFIG_HZ;
}

// Milliseconds -> Nifties
static inline uint64_t msecs_to_nifties(uint64_t msecs) {
    return (msecs * CONFIG_HZ) / 1000ULL;
}

#endif
