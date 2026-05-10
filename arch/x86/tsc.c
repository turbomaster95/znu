#include <stdint.h>
#include <stdbool.h>
#include <tsc.h>
#include <pi.h> 
#include <stdlib.h>
#include <limine.h>
#include <cpuid.h>

tsc_info_t tsc_data = {
    .supported = false,
    .frequency = 0
};

static uint64_t tsc_calibrate_frequency(void);

tsc_info_t tsc_detect(void) {
    debugln("[TSC] Detecting TSC...");

    // 1. Check max supported leaf via CPUID_GETVENDORSTRING (Leaf 0)
    cpuid_res_t basic_info = cpuid_query(CPUID_GETVENDORSTRING, 0);
    if (basic_info.eax < 1) { 
        debugln("[TSC] CPUID leaf 1 not supported.");
        tsc_data.supported = false;
        tsc_data.frequency = 0;
        return tsc_data;
    }

    // 2. Check for TSC support (Leaf 1, EDX bit 4)
    // Offset mapping for your helper: eax=0, ebx=1, ecx=2, edx=3
    if (cpuid_has_feature(CPUID_GETFEATURES, 3, 4)) { 
        tsc_data.supported = true;
        debugln("[TSC] TSC supported.");

        /* 
         * Logic: We check if Leaf 0x15 is available for hardware-provided frequency.
         * If not, we fall back to the manual PIT calibration.
         */
        if (basic_info.eax >= CPUID_GETTSC_INFO) {
            cpuid_res_t tsc_info = cpuid_query(CPUID_GETTSC_INFO, 0);
            
            // If EAX/EBX are non-zero, we can calculate freq without PIT
            if (tsc_info.eax != 0 && tsc_info.ebx != 0) {
                // Freq = Crystal * (EBX/EAX)
                // Note: ECX is the crystal frequency in Hz (if provided)
                uint64_t crystal = tsc_info.ecx ? tsc_info.ecx : 24000000; // Default 24MHz
                tsc_data.frequency = (crystal * tsc_info.ebx) / tsc_info.eax;
                debugln("[TSC] Hardware-reported frequency: %llu Hz", tsc_data.frequency);
            }
        }

        // Fallback to PIT calibration if hardware didn't report it
        if (tsc_data.frequency == 0) {
            tsc_data.frequency = tsc_calibrate_frequency();
        }

        if (tsc_data.frequency == 0) {
            debugwarn("[TSC] Failed to calibrate frequency, using placeholder.");
            tsc_data.frequency = 3000000000; // 3 GHz
        } else {
            debugln("[TSC] Final TSC frequency: %llu Hz", tsc_data.frequency);
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
    debugln("[TSC] Calibrating TSC frequency via hardware PIT polling...");

    /* 1. Prepare the PIT: Channel 2, LSB/MSB, mode 0 (Interrupt on terminal count) */
    /* We use Channel 2 because it's safer to poll without messing up the system timer */
    outb(0x61, (inb(0x61) & ~0x02) | 0x01); // Gate on for Channel 2
    outb(0x43, 0xB0);                       // Select Channel 2, LSB/MSB, Mode 0

    // Set PIT reload value to 0xFFFF (65535)
    // The PIT frequency is 1.193182 MHz. 
    // 0xFFFF ticks is approx 54.925 ms.
    outb(0x42, 0xFF); 
    outb(0x42, 0xFF);

    uint64_t tsc_start = tsc_read();

    /* 2. Wait for the PIT to count down */
    /* We poll the status of Channel 2 until the OUT bit (bit 7) goes high */
    while (!(inb(0x61) & 0x20)); 

    uint64_t tsc_end = tsc_read();

    /* 3. Cleanup: Disable PIT Channel 2 gate */
    outb(0x61, inb(0x61) & ~0x01);

    uint64_t tsc_cycles = tsc_end - tsc_start;

    /* 
     * 4. Math:
     * PIT Frequency = 1193182 Hz
     * Ticks used = 65535
     * Seconds elapsed = 65535 / 1193182 (~0.0549 sec)
     * Freq = Cycles / (Ticks / PIT_FREQ) = (Cycles * PIT_FREQ) / Ticks
     */
    uint64_t frequency = (tsc_cycles * 1193182) / 65535;

    debugln("[TSC] Calibration complete: %llu Hz", frequency);
    return frequency;
}
