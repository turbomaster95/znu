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
    debugln("[timekeeper] Initializing timekeeping components...");

    tsc_detect(); 

    if (tsc_data.supported && tsc_data.frequency > 0) {
        debugln("[timekeeper] Using TSC frequency as CPU speed.");
    } else {
        measure_cpu_speed(); 
    }

    lapic_init(0);
    calibrate_lapic_timer_no_irq(); 

    if (rtc_init()) {
        rtc_time_t boot_time;
        if (rtc_read_time(&boot_time)) {
            debugln("[timekeeper] RTC Boot Time: %02d/%02d/20%02d %02d:%02d:%02d",
                    boot_time.day, boot_time.month, boot_time.year,
                    boot_time.hour, boot_time.minute, boot_time.second);
            
        } else {
            debugwarn("[timekeeper] RTC read failed.");
        }
    } else {
        debugwarn("[timekeeper] RTC hardware not responding.");
    }

    debugln("[timekeeper] Init done!");
}

PERFORM void timekeeper_on_tick(void) {
    system_nanoseconds += 1000000; // 1ms = 1,000,000ns
}

PERFORM uint64_t timekeeper_timefromboot(void) {
    return system_nanoseconds;
}
