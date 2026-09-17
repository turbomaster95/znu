#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <stdint.h>
#include <uacpi/uacpi.h>
#include <generated/nifties.h>

extern volatile uint64_t nifties;

void timekeeper_on_tick(void);
void timekeeper_init(void);
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void);
uint64_t timekeeper_timefromboot(void);

// Milliseconds -> Nifties
static inline uint64_t msecs_to_nifties(uint64_t msecs) {
    return (msecs * MSEC_TO_NIFTIES_MUL) >> MSEC_TO_NIFTIES_SHR;
}

#endif
