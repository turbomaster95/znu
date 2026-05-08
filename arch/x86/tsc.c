#include <stdint.h>
#include <stdbool.h>
#include <tsc.h>
#include <pi.h> 
#include <stdlib.h>
#include <limine.h>

tsc_info_t tsc_data = {
    .supported = false,
    .frequency = 0
};

static uint64_t tsc_calibrate_frequency(void);

tsc_info_t tsc_detect(void) {
    debugln("[TSC] Detecting TSC...");

    uint32_t eax, ebx, ecx, edx;

    // Check for CPUID support (leaf 0)
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    if (eax < 1) { // CPUID leaf 1 is not supported
        debugln("[TSC] CPUID leaf 1 not supported.");
        tsc_data.supported = false;
        tsc_data.frequency = 0;
        return tsc_data;
    }

    // Check for TSC support (leaf 1, ECX bit 4)
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    if (ecx & (1 << 4)) { // TSC flag is set
        tsc_data.supported = true;
        debugln("[TSC] TSC supported.");

        // Calibrate TSC frequency using PIT
        tsc_data.frequency = tsc_calibrate_frequency();
        if (tsc_data.frequency == 0) {
            debugwarn("[TSC] Failed to calibrate TSC frequency, using placeholder.");
            // Fallback frequency if calibration fails
            tsc_data.frequency = 3000000000; // Placeholder: 3 GHz (adjust later)
        } else {
            debugln("[TSC] Calibrated TSC frequency: %llu Hz", tsc_data.frequency);
        }
    } else {
        debugln("[TSC] TSC not supported.");
        tsc_data.supported = false;
        tsc_data.frequency = 0;
    }

    return tsc_data;
}

uint64_t tsc_read(void) {
    if (!tsc_data.supported) {
        return 0; // TSC not supported or not detected
    }

    // Use inline assembly to read the TSC (RDTSC instruction)
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

// Function to get the TSC frequency in Hz
uint64_t tsc_get_frequency(void) {
    if (tsc_data.frequency == 0 && tsc_data.supported) {
        tsc_data.frequency = tsc_calibrate_frequency();
        if (tsc_data.frequency == 0) {
             debugwarn("[TSC] Failed to get TSC frequency after detection attempt, returning 0.");
             return 0;
        }
    } else if (!tsc_data.supported) {
        debugerr("[TSC] TSC is not supported, cannot get frequency.");
        return 0;
    }
    return tsc_data.frequency;
}

static uint64_t tsc_calibrate_frequency(void) {
    debugln("[TSC] Calibrating TSC frequency using PIT...");

    const uint32_t calibration_duration_ms = 10; // 10 milliseconds
    const uint32_t pit_frequency = 1000; // PIT set to 1kHz for ~1ms interrupts

    // Record current timer_ticks to measure elapsed time accurately.
    // We need to ensure timer_ticks is updated by PIT interrupts.
    uint64_t start_timer_ticks = timer_ticks;
    
    // Read TSC at the start
    uint64_t tsc_start = tsc_read();

    // Wait for the specified duration using msleep.
    // msleep relies on timer_ticks and PIT interrupts.
    msleep(calibration_duration_ms);

    // Read TSC at the end
    uint64_t tsc_end = tsc_read();

    // Calculate elapsed TSC cycles
    uint64_t tsc_cycles = tsc_end - tsc_start;

    // Calculate duration in seconds.
    // msleep waits for timer_ticks to increment by calibration_duration_ms.
    // If PIT frequency is 1000 Hz, 1 tick is 1ms.
    // So, the actual duration in seconds is calibration_duration_ms / 1000.0
    uint64_t duration_seconds = calibration_duration_ms / 1000ULL;
    if (duration_seconds == 0) duration_seconds = 1; // Avoid division by zero if ms < 1000

    // Frequency = cycles / seconds
    uint64_t frequency = tsc_cycles / duration_seconds;

    debugln("[TSC] Calibration: %llu cycles in %u ms.", tsc_cycles, calibration_duration_ms);
    
    return frequency;
}
