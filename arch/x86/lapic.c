#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limine.h> // For limine_lapic_request
#include <stdio.h>
#include <pi.h>
#include <lapic.h>
#include <prelude.h>

extern volatile uint64_t timer_ticks;

extern volatile struct limine_hhdm_request hhdm_request;

// LAPIC Base Address (obtained from Limine)
volatile uint64_t* lapic_base = NULL;

// LAPIC Timer Interrupt Vector.
#define LAPIC_TIMER_VECTOR 48

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

    // Request LAPIC info from Limine
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));

    debugln("[lapic] Pulled HHDM from Limine");

    uintptr_t lapic_phys = (lo & 0xFFFFF000) | ((uint64_t)(hi & 0x0F) << 32);

    lapic_base = (volatile uint64_t*)(lapic_phys + hhdm_request.response->offset);

    debugln("[dlapic] LAPIC Base (Phys): 0x%p", lapic_phys);
    debugln("[dlapic] LAPIC Base (Virt): 0x%p", lapic_base);

    // Enable LAPIC globally via MSR
    // Ensure lo and hi are explicitly 32-bit for the registers
    debugln("[dlapic] Read lo, and hi");
    // Set bit 11 (Enable)
    lo |= (1 << 11);

    // Write it back
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(0x1B));
    debugln("[dlapic] Wrote lo, hi after bit");

    // Software-enable LAPIC (SVR bit 8)
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    lapic_write(LAPIC_REG_SVR, lapic_read(LAPIC_REG_SVR) | 0x100 | 0xFF);
    debugln("[lapic] Survived the LAPIC SVR write!");

    // LINT0: Virtual Wire Mode (ExtINT)
    lapic_write(0x350, 0x700);
}

PERFORM void calibrate_lapic_timer_no_irq() {
    debugln("[clapic] Polling PIT for LAPIC calibration...");
    
    // PIT Channel 2 is best for this to avoid messing with system time
    // But if you use Channel 0, ensure it's in Mode 2 or 3.
    pit_init(1000); 

    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x03); // Divide by 16
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

    // Get start state
    uint16_t start_pit = read_pit_count();
    uint32_t start_lapic = lapic_read(LAPIC_REG_CURRENT_COUNT);

    // Wait for ~10ms (roughly 11932 PIT ticks)
    // 1.193MHz / 100 = 11931.8
    uint32_t ticks_to_wait = 11932;
    while (1) {
        uint16_t current_pit = read_pit_count();
        uint32_t diff = (start_pit >= current_pit) ? (start_pit - current_pit) : (start_pit + (0xFFFF - current_pit));
        if (diff > ticks_to_wait) break;
    }

    uint32_t end_lapic = lapic_read(LAPIC_REG_CURRENT_COUNT);
    uint32_t lapic_ticks_elapsed = start_lapic - end_lapic;

    // We waited 10ms, so divide by 10 to get ticks per 1ms
    lapic_ticks_per_ms = lapic_ticks_elapsed / 10;

    debugln("[lapic] LAPIC Ticks per ms: %u", lapic_ticks_per_ms);
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
        __asm__ volatile("pause"); // Hints to the CPU we are in a spin-loop
    }

    uint32_t lapic_end_count = lapic_read(LAPIC_REG_CURRENT_COUNT);

    uint32_t ticks_elapsed;
    if (lapic_end_count <= lapic_start_count) {
        ticks_elapsed = lapic_start_count - lapic_end_count;
    } else {
        // Timer wrapped around (0xFFFFFFFF -> 0).
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

// C function for the LAPIC timer ISR
PERFORM void lapic_timer_isr() {
    lapic_eoi();
}

PERFORM void sleep(uint32_t ms) {
    if (!lapic_base) {
        debugerr("LAPIC not initialized, cannot sleep.");
        return;
    }
    if (lapic_ticks_per_ms == 0) {
        debugwarn("LAPIC timer not calibrated, cannot sleep accurately.");
        return;
    }

    lapic_write(LAPIC_REG_DIVIDE_CONF, LAPIC_TIMER_DIVIDER_CONF);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR);

    uint32_t ticks_to_wait = ms * lapic_ticks_per_ms;
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    
    lapic_timer_fired = false;
    lapic_write(LAPIC_REG_INITIAL_COUNT, ticks_to_wait);

    while (!lapic_timer_fired) {
        __asm__ volatile("hlt");
    }
}

void lapic_timer_test() {
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x03); 
    lapic_write(LAPIC_REG_LVT_TIMER, 32 | (1 << 17)); 
    lapic_write(LAPIC_REG_INITIAL_COUNT, 1000000); 
}

