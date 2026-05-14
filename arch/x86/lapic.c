#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limine.h>
#include <stdio.h>
#include <pi.h>
#include <tsc.h>
#include <lapic.h>
#include <prelude.h>

extern volatile uint64_t timer_ticks;

extern volatile struct limine_hhdm_request hhdm_request;

volatile uint64_t* lapic_base = NULL;

#define LAPIC_TIMER_CLOCK_HZ 24000000
#define LAPIC_TIMER_DIVIDER_CONF 0x3
#define LAPIC_TIMER_DIVIDER 16

volatile uint32_t lapic_ticks_per_ms = 0;
volatile bool lapic_timer_fired = false;

PERFORM uint32_t lapic_read(uint32_t offset) {
    return *(volatile uint32_t*)((uintptr_t)lapic_base + offset);
}

PERFORM void lapic_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((uintptr_t)lapic_base + offset) = value;
}

void lapic_init() {
    if (lapic_base != NULL) return; // Already initialized

    if (!hhdm_request.response) {
        debugerr("HHDM NOT FOUND");
        return; 
    } else {
        debugln("[lapic] HHDM response found!");
    }

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));

    debugln("[lapic] Pulled HHDM from Limine");

    uintptr_t lapic_phys = (lo & 0xFFFFF000) | ((uint64_t)(hi & 0x0F) << 32);

    lapic_base = (volatile uint64_t*)(lapic_phys + hhdm_request.response->offset);

    debugln("[dlapic] LAPIC Base (Phys): 0x%p", lapic_phys);
    debugln("[dlapic] LAPIC Base (Virt): 0x%p", lapic_base);

    debugln("[dlapic] Read lo, and hi");
    // Set bit 11 (Enable)
    lo |= (1 << 11);

    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(0x1B));
    debugln("[dlapic] Wrote lo, hi after bit");

    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    lapic_write(LAPIC_REG_SVR, lapic_read(LAPIC_REG_SVR) | 0x100 | 0xFF);
    debugln("[lapic] Survived the LAPIC SVR write!");

    lapic_write(0x350, 0x700);
}

PERFORM void calibrate_lapic_timer_no_irq() {
    debugln("[clapic] Calibrating LAPIC timer using TSC...");

    extern tsc_info_t tsc_data;

    if (!tsc_data.supported || tsc_data.frequency == 0) {
        debugwarn("[clapic] TSC unavailable, falling back to PIT ch2 polling.");
        outb(0x61, (inb(0x61) & ~0x02) | 0x01); // gate on, speaker off
        outb(0x43, 0xB0);                        // ch2, LSB+MSB, mode 0
        outb(0x42, 0xFF);
        outb(0x42, 0xFF);

        lapic_write(LAPIC_REG_DIVIDE_CONF,   0x03);
        lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

        while (!(inb(0x61) & 0x20));

        uint32_t lapic_remaining = lapic_read(LAPIC_REG_CURRENT_COUNT);
        lapic_write(LAPIC_REG_INITIAL_COUNT, 0); // stop timer
        outb(0x61, inb(0x61) & ~0x01);           // gate off

        uint32_t elapsed = 0xFFFFFFFF - lapic_remaining;
        lapic_ticks_per_ms = (uint32_t)(((uint64_t)elapsed * 1193182ULL)
                                        / (65535ULL * 1000ULL));
        debugln("[lapic] LAPIC calibration (PIT fallback): %u ticks/ms",
                lapic_ticks_per_ms);
        return;
    }

    const uint32_t MEASURE_MS = 10;
    uint64_t tsc_per_ms = tsc_data.frequency / 1000ULL;
    uint64_t tsc_wait   = tsc_per_ms * (uint64_t)MEASURE_MS;

    lapic_write(LAPIC_REG_DIVIDE_CONF,   0x03);
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

    uint64_t tsc_start   = tsc_read();
    uint32_t lapic_start = lapic_read(LAPIC_REG_CURRENT_COUNT);

    while ((tsc_read() - tsc_start) < tsc_wait) {
        __asm__ volatile("pause");
    }

    uint32_t lapic_end = lapic_read(LAPIC_REG_CURRENT_COUNT);

    lapic_write(LAPIC_REG_INITIAL_COUNT, 0);

    uint32_t lapic_elapsed = lapic_start - lapic_end;
    lapic_ticks_per_ms     = lapic_elapsed / MEASURE_MS;

    debugln("[lapic] LAPIC calibration: %u ticks in %u ms -> %u ticks/ms",
            lapic_elapsed, MEASURE_MS, lapic_ticks_per_ms);
}


PERFORM void calibrate_lapic_timer() {
    if (!lapic_base) {
        debugerr("LAPIC not initialized, cannot calibrate timer.");
        return;
    }
    if (lapic_ticks_per_ms != 0) {
        debugln("[lapic] LAPIC timer already calibrated.");
        return; // Already calibrated
    }

    debugln("[clapic] Calibrating LAPIC timer...");

    uint64_t pit_start_ticks = timer_ticks;
    uint64_t pit_wait_duration_ticks = 100; // Wait for ~1 second if PIT is 1kHz.

    lapic_write(LAPIC_REG_DIVIDE_CONF, LAPIC_TIMER_DIVIDER_CONF);
    uint32_t lvt_timer_value = (32); // One-shot, vector 255 (arbitrary, non-interrupting)
    lapic_write(LAPIC_REG_LVT_TIMER, lvt_timer_value);
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

    uint32_t lapic_start_count = lapic_read(LAPIC_REG_CURRENT_COUNT);

    uint64_t current_pit_ticks;
    while (timer_ticks < pit_start_ticks + pit_wait_duration_ticks) {
        __asm__ volatile("pause");
    }

    uint32_t lapic_end_count = lapic_read(LAPIC_REG_CURRENT_COUNT);

    uint32_t ticks_elapsed;
    if (lapic_end_count <= lapic_start_count) {
        ticks_elapsed = lapic_start_count - lapic_end_count;
    } else {
        ticks_elapsed = (0xFFFFFFFF - lapic_start_count) + lapic_end_count + 1;
    }

    uint32_t pit_frequency_hz = 1000; // From pit_init(1000)
    uint32_t duration_ms = (pit_wait_duration_ticks * 1000) / pit_frequency_hz;

    if (duration_ms == 0) duration_ms = 1; // Avoid division by zero

    lapic_ticks_per_ms = ticks_elapsed / duration_ms;

    debugln("[clapic] APIC timer calibrated: %u ticks/ms.", lapic_ticks_per_ms);

    lapic_write(LAPIC_REG_INITIAL_COUNT, 0);
    lapic_write(LAPIC_REG_LVT_TIMER, 0x10000); // Disable timer (bit 16)
}


void lapic_eoi() {
    if (lapic_base) {
        *(volatile uint32_t*)((uintptr_t)lapic_base + LAPIC_REG_EOI) = 0;
    }
}

extern void lapic_timer_isr_wrapper(void);

PERFORM void lapic_timer_isr() {
    lapic_eoi();
}

PERFORM void sleep(uint32_t ms) {
    extern volatile uint64_t timer_ticks;
    uint64_t target = timer_ticks + (uint64_t)ms;
    while (timer_ticks < target) {
        __asm__ volatile("sti; hlt");
    }
}

void lapic_timer_test() {
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x03); 
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | (1 << 17)); 
    lapic_write(LAPIC_REG_INITIAL_COUNT, 1000000); 
}
