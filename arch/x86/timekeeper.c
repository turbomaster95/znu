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
extern void calibrate_lapic_timer_no_irq(void);

static volatile uint64_t system_nanoseconds = 0;

void timekeeper_init(void) {
    debugln("Initializing timekeeping components...");

    // 1. Detect and Calibrate the TSC
    tsc_detect(); 

    if (tsc_data.supported && tsc_data.frequency > 0) {
        // We already have 2.3GHz from the polling calibration!
        // No need to risk an msleep() hang here.
        debugln("[Timekeeper] Using TSC frequency as CPU speed.");
    } else {
        // Only measure if TSC detection failed
        measure_cpu_speed(); 
    }

    // 2. Initialize Local APIC Timer
    // The LAPIC timer usually runs at the 'Bus Frequency', not the CPU core frequency.
    lapic_init();
    calibrate_lapic_timer_no_irq(); 

    // 3. Wall Clock Initialization (RTC)
    if (rtc_init()) {
        rtc_time_t boot_time;
        if (rtc_read_time(&boot_time)) {
            debugln("[Timekeeper] RTC Boot Time: %02d/%02d/20%02d %02d:%02d:%02d",
                    boot_time.day, boot_time.month, boot_time.year,
                    boot_time.hour, boot_time.minute, boot_time.second);
            
            // Suggestion: Implement a ymd_to_unix_timestamp() here 
            // to set a global 'system_boot_unix_time' variable.
        } else {
            debugwarn("[Timekeeper] RTC read failed.");
        }
    } else {
        debugwarn("[Timekeeper] RTC hardware not responding.");
    }

    debugln("Timekeeping components initialized.");
}

PERFORM void timekeeper_on_tick(void) {
    system_nanoseconds += 1000000; // 1ms = 1,000,000ns
}

PERFORM uint64_t timekeeper_timefromboot(void) {
    return system_nanoseconds;
}


