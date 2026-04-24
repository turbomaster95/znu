#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <stdint.h>
#include <uacpi/uacpi.h>

void timekeeper_on_tick(void);
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void);
uint64_t timekeeper_timefromboot(void);

#endif
