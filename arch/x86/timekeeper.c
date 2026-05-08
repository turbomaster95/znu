#include <stdint.h>
#include <timekeeper.h>
#include <uacpi/types.h>
#include <prelude.h>
#include <rtc.h>
#include <lapic.h>
#include <tsc.h>
#include <stdlib.h>

extern uint32_t lapic_ticks_per_ms;
extern tsc_info_t tsc_data;
extern uint64_t measure_cpu_speed(void);

static volatile uint64_t system_nanoseconds = 0;

void timekeeper_init(void) {
    debugln("Initializing timekeeping components...");

    // 1. Initialize and detect TSC
    tsc_detect(); // This should set tsc_data.supported and tsc_data.frequency

    // 2. Measure CPU speed using TSC and PIT (if TSC is supported)
    uint64_t cpu_speed = 0;
    if (tsc_data.supported) {
        cpu_speed = measure_cpu_speed();
        if (cpu_speed > 0) {
            // Update tsc_data.frequency if measurement was successful and is more accurate
            // For now, we rely on the calibration within measure_cpu_speed if it returns non-zero.
            // If tsc_data.frequency was placeholder, update it.
            if (tsc_data.frequency == 3000000000) { // Check if it's the placeholder value
                tsc_data.frequency = cpu_speed;
                debugln("[Timekeeper] Updated TSC frequency from measurement: %llu Hz", tsc_data.frequency);
            }
        }
    } else {
        debugwarn("[Timekeeper] TSC not supported. CPU speed measurement skipped.");
    }
    // 3. Initialize and calibrate LAPIC timer
    lapic_init();
    // Calibrate LAPIC timer, potentially using the measured CPU speed for more accuracy
    // or by comparing against a known frequency. The current calibration relies on PIT.
    calibrate_lapic_timer(); // This function uses pit_init and timer_ticks.

    // 4. Initialize RTC and set initial system time
    if (rtc_init()) {
        rtc_time_t boot_time;
        if (rtc_read_time(&boot_time)) {
            // Convert RTC time to a system time format (e.g., Unix timestamp)
            // This requires a date/time conversion function and a base epoch.
            // For now, just log the read time.
            debugln("[Timekeeper] Initial system time from RTC: C%d Y%02d M%02d D%02d %02d:%02d:%02d",
                    boot_time.century, boot_time.year, boot_time.month, boot_time.day,
                    boot_time.hour, boot_time.minute, boot_time.second);
            
            // TODO: Convert this to a system-wide timestamp and set system time.
            // This would involve setting timer_ticks or system_nanoseconds appropriately.
            // For now, it's just read and logged.
        } else {
            debugwarn("[Timekeeper] Failed to read time from RTC.");
        }
    } else {
        debugwarn("[Timekeeper] RTC initialization failed. System time will not be set from RTC.");
    }

    debugln("Timekeeping components initialized.");
}
PERFORM void timekeeper_on_tick(void) {
    system_nanoseconds += 1000000; // 1ms = 1,000,000ns
}


PERFORM uint64_t timekeeper_timefromboot(void) {
    return system_nanoseconds;
}


