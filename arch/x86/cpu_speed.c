#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For NULL
#include <tsc.h>
#include <rtc.h>
#include <pi.h>
#include <timekeeper.h> 
#include <stdlib.h>

extern void pit_init(uint32_t frequency);
extern void msleep(uint32_t ms);
extern tsc_info_t tsc_data;

uint64_t measure_cpu_speed(void) {
    debugln("[CPU] Measuring CPU speed...");

    const uint32_t ms_to_wait = 10;
    
    if (!tsc_data.supported) {
        debugerr("[CPU] TSC not supported, cannot measure speed.");
        return 0;
    }

    // Ensure PIT is ready
    pit_init(1000); 

    // Warm up the pipeline/cache
    tsc_read();

    uint64_t tsc_start = tsc_read();
    msleep(ms_to_wait);
    uint64_t tsc_end = tsc_read();

    uint64_t tsc_cycles = tsc_end - tsc_start;

    // Fixed Math: (Cycles * 1000) / ms = Cycles per Second (Hz)
    uint64_t cpu_frequency = (tsc_cycles * 1000) / ms_to_wait;

    debugln("[CPU] Measured %llu cycles in %u ms", tsc_cycles, ms_to_wait);
    debugln("[CPU] Estimated frequency: %llu Hz (%llu MHz)", 
            cpu_frequency, cpu_frequency / 1000000);

    return cpu_frequency;
}
