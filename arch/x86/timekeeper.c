#include <stdint.h>
#include <timekeeper.h>
#include <uacpi/types.h>
#include <prelude.h>

// This is the variable defined in your lapic/timer code
extern uint32_t lapic_ticks_per_ms;

static volatile uint64_t system_nanoseconds = 0;

/**
 * Call this inside your LAPIC interrupt handler.
 * Assuming you set LAPIC_REG_INITIAL_COUNT to lapic_ticks_per_ms,
 * this fires every 1ms.
 */
PERFORM void timekeeper_on_tick(void) {
    system_nanoseconds += 1000000; // 1ms = 1,000,000ns
}


PERFORM uint64_t timekeeper_timefromboot(void) {
    return system_nanoseconds;
}
