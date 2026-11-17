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
    p_info("tsc", "Detecting TSC...");

    if (cpu_has(MAX_STANDARD_LEAF) < 1) { 
        p_warn("tsc", "CPUID leaf 1 not supported.");
        tsc_data.supported = false;
        tsc_data.frequency = 0;
        return tsc_data;
    }

    if (cpu_has(TSC_SUPPORT)) { 
        tsc_data.supported = true;
        p_info("tsc", "TSC supported.");

        if (cpu_has(MAX_STANDARD_LEAF) >= CPUID_TSC_INFO) {
            cpuid_res_t tsc_info = cpuid_query(CPUID_TSC_INFO, 0);
            
            if (tsc_info.eax != 0 && tsc_info.ebx != 0) {
                uint64_t crystal = tsc_info.ecx ? tsc_info.ecx : 24000000; // Default 24MHz
                tsc_data.frequency = (crystal * tsc_info.ebx) / tsc_info.eax;
                p_debug("tsc", "Hardware-reported frequency: %llu Hz", tsc_data.frequency);
            }
        }

        if (tsc_data.frequency == 0) {
            tsc_data.frequency = tsc_calibrate_frequency();
        }

        if (tsc_data.frequency == 0) {
            debugwarn("[TSC] Failed to calibrate frequency, using placeholder.");
            tsc_data.frequency = 3000000000; // 3 GHz
        } else {
            p_info("tsc", "Final TSC frequency: %llu Hz", tsc_data.frequency);
        }
    } else {
        p_warn("tsc", "TSC not supported.");
        tsc_data.supported = false;
        tsc_data.frequency = 0;
    }

    return tsc_data;
}

uint64_t tsc_read(void) {
    if (!tsc_data.supported) {
        return 0; 
    }

    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

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
    p_debug("tsc", "Calibrating TSC frequency via hardware PIT polling...");

    outb(0x61, (inb(0x61) & ~0x02) | 0x01); // Gate on for Channel 2
    outb(0x43, 0xB0);                       // Select Channel 2, LSB/MSB, Mode 0

    outb(0x42, 0xFF); 
    outb(0x42, 0xFF);

    uint64_t tsc_start = tsc_read();

    while (!(inb(0x61) & 0x20)); 

    uint64_t tsc_end = tsc_read();

    outb(0x61, inb(0x61) & ~0x01);

    uint64_t tsc_cycles = tsc_end - tsc_start;

    uint64_t frequency = (tsc_cycles * 1193182) / 65535;

    p_info("tsc", "Calibration complete: %llu Hz", frequency);
    return frequency;
}
