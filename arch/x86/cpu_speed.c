#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For NULL
#include <tsc.h>
#include <rtc.h>
#include <pi.h>
#include <timekeeper.h> 
#include <stdlib.h>

extern tsc_info_t tsc_detect(void);
extern bool rtc_init(void);
extern bool rtc_read_time(rtc_time_t *time);
extern void pit_init(uint32_t frequency);
extern void msleep(uint32_t ms);
extern void lapic_init(void);
extern void calibrate_lapic_timer(void); // Or a similar function to calibrate LAPIC
extern tsc_info_t tsc_data;

// Function to measure CPU speed by counting TSC cycles during a PIT delay
uint64_t measure_cpu_speed(void) {
    debugln("[CPU] Measuring CPU speed...");

    const uint32_t calibration_duration_ms = 10; // 10 milliseconds for measurement
    const uint32_t pit_frequency = 1000; // PIT set to 1kHz for ~1ms ticks

    if (!tsc_data.supported) { // Assuming tsc_data is globally accessible or passed
        debugerr("[CPU] TSC not supported, cannot measure speed.");
        return 0;
    }

    pit_init(pit_frequency);

    uint64_t tsc_start = tsc_read();
    uint64_t start_timer_ticks = timer_ticks; // Assuming timer_ticks is accessible

    msleep(calibration_duration_ms);

    uint64_t tsc_end = tsc_read();

    uint64_t tsc_cycles = tsc_end - tsc_start;

    uint64_t duration_seconds = calibration_duration_ms / 1000ULL;
    if (duration_seconds == 0) duration_seconds = 1;

    // CPU speed = cycles / seconds
    uint64_t cpu_frequency = tsc_cycles / duration_seconds;

    debugln("[CPU] Measured TSC cycles in %u ms: %llu", calibration_duration_ms, tsc_cycles);
    debugln("[CPU] Estimated CPU frequency: %llu Hz", cpu_frequency);

    return cpu_frequency;
}
