// arch/x86/lapic.c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limine.h> // For limine_lapic_request
#include <stdio.h>
#include <pi.h>

extern volatile uint64_t timer_ticks;

extern volatile struct limine_hhdm_request hhdm_request;

// LAPIC Base Address (obtained from Limine)
volatile uint64_t* lapic_base = NULL;

// LAPIC Register Offsets
#define LAPIC_REG_ID          0x0020
#define LAPIC_REG_SVR         0x00F0 // Spurious Interrupt Vector Register
#define LAPIC_REG_LVT_TIMER  0x00320
#define LAPIC_REG_INITIAL_COUNT 0x00380
#define LAPIC_REG_CURRENT_COUNT 0x00390
#define LAPIC_REG_DIVIDE_CONF 0x003E0
#define LAPIC_REG_EOI         0x00B0

// LAPIC Timer Interrupt Vector. We will use vector 32 to replace the PIT timer handler.
#define LAPIC_TIMER_VECTOR 32

// --- Constants for LAPIC Timer ---
// Define LAPIC timer clock frequency (example: 24MHz).
// This value is often board-specific or system-specific and should ideally be determined accurately.
#define LAPIC_TIMER_CLOCK_HZ 24000000
// Define the divider for the LAPIC timer. The value 0x3 in the DIVIDE_CONF register
// is assumed here to correspond to a divider of 16, based on previous examples.
#define LAPIC_TIMER_DIVIDER_CONF 0x3
// The actual divider value derived from LAPIC_TIMER_DIVIDER_CONF (0x3 -> 16)
#define LAPIC_TIMER_DIVIDER 16

// Global variable to store calibrated ticks per millisecond for LAPIC timer
volatile uint32_t lapic_ticks_per_ms = 0;

// --- Helper functions for LAPIC MMIO access ---
static inline uint32_t lapic_read(uint32_t offset) {
    return *(volatile uint32_t*)((uintptr_t)lapic_base + offset);
}

static inline void lapic_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((uintptr_t)lapic_base + offset) = value;
}

// --- LAPIC Initialization ---
void lapic_init() {
    if (lapic_base != NULL) return; // Already initialized

    if (!hhdm_request.response) {
        debugln("HHDM NOT FOUND");
        return; 
    } else {
        debugln("HHDM response found!");
    }

    // Request LAPIC info from Limine
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));

    debugln("Got HHDM from Limine");

    uintptr_t lapic_phys = (lo & 0xFFFFF000) | ((uint64_t)(hi & 0x0F) << 32);

    lapic_base = (volatile uint64_t*)(lapic_phys + hhdm_request.response->offset);

    debugln("LAPIC Base (Phys): 0x%p", lapic_phys);
    debugln("LAPIC Base (Virt): 0x%p", lapic_base);

    // Enable LAPIC globally via MSR
    // Ensure lo and hi are explicitly 32-bit for the registers
    debugln("Read lo, and hi");
    // Set bit 11 (Enable)
    lo |= (1 << 11);

    // Write it back
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(0x1B));
    debugln("Wrote lo, hi after bit");

    // Software-enable LAPIC (SVR bit 8)
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    lapic_write(LAPIC_REG_SVR, lapic_read(LAPIC_REG_SVR) | 0x100 | 0xFF);
    debugln("Survived the write?");
}

// --- LAPIC Timer Calibration ---
// Calibrates the LAPIC timer by comparing it against the PIT.
// Assumes PIT is initialized to a known frequency (e.g., 1000 Hz) and interrupts are enabled.
void calibrate_lapic_timer_no_irq() {
    pit_init(1000); // 1ms per wrap roughly, but we'll use the raw count
    
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x03); // Divide by 16
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

    uint16_t start_pit = read_pit_count();
    uint32_t start_lapic = lapic_read(LAPIC_REG_CURRENT_COUNT);

    // Wait for the PIT to decrease by ~1000 ticks 
    // (PIT frequency is 1.193MHz, so 1193 ticks is ~1ms)
    while (1) {
        uint16_t current_pit = read_pit_count();
        // PIT counts down, so we check the difference
        if ((uint16_t)(start_pit - current_pit) > 1193) break; 
    }

    uint32_t end_lapic = lapic_read(LAPIC_REG_CURRENT_COUNT);
    lapic_ticks_per_ms = start_lapic - end_lapic;

    debugln("LAPIC Ticks per ms: %u", lapic_ticks_per_ms);
}


void calibrate_lapic_timer() {
    if (!lapic_base) {
        debugln("LAPIC not initialized, cannot calibrate timer.");
        return;
    }
    if (lapic_ticks_per_ms != 0) {
        debugln("LAPIC timer already calibrated.");
        return; // Already calibrated
    }

    debugln("Calibrating LAPIC timer...");

    // Ensure PIT is initialized and interrupts are enabled.
    // We'll wait for 1000 PIT ticks, which is approximately 1 second if PIT is 1kHz.
    uint64_t pit_start_ticks = timer_ticks;
    uint64_t pit_wait_duration_ticks = 100; // Wait for ~1 second if PIT is 1kHz.

    // 1. Configure LAPIC Timer for One-Shot mode for calibration.
    lapic_write(LAPIC_REG_DIVIDE_CONF, LAPIC_TIMER_DIVIDER_CONF);
    // Set LVT Timer Register: One-Shot Mode (bit 17=0) | Delivery Vector.
    // Using a temporary vector (e.g., 0xFF) that won't conflict or trigger real interrupts.
    uint32_t lvt_timer_value = (32); // One-shot, vector 255 (arbitrary, non-interrupting)
    lapic_write(LAPIC_REG_LVT_TIMER, lvt_timer_value);
    // Set a large initial count to ensure it doesn't expire immediately.
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

    // 2. Record LAPIC timer's current count *before* waiting.
    uint32_t lapic_start_count = lapic_read(LAPIC_REG_CURRENT_COUNT);

    // 3. Wait for the PIT to tick N times.
    //    This busy-wait using HLT is acceptable for calibration itself.
    uint64_t current_pit_ticks;
    do {
        current_pit_ticks = timer_ticks; // Read timer_ticks inside loop
        __asm__ volatile("hlt"); // Pause CPU, wait for PIT interrupt
    } while (current_pit_ticks < pit_start_ticks + pit_wait_duration_ticks);


    // 4. Read LAPIC timer's current count *after* waiting.
    uint32_t lapic_end_count = lapic_read(LAPIC_REG_CURRENT_COUNT);

    // 5. Calculate ticks elapsed.
    uint32_t ticks_elapsed;
    if (lapic_end_count <= lapic_start_count) {
        ticks_elapsed = lapic_start_count - lapic_end_count;
    } else {
        // Timer wrapped around (0xFFFFFFFF -> 0).
        ticks_elapsed = (0xFFFFFFFF - lapic_start_count) + lapic_end_count + 1;
    }

    // 6. Calculate ticks per ms.
    //    Duration in ms = (pit_wait_duration_ticks / PIT_FREQUENCY) * 1000
    //    Assuming PIT frequency is 1000 Hz from pit_init(1000).
    uint32_t pit_frequency_hz = 1000; // From pit_init(1000)
    uint32_t duration_ms = (pit_wait_duration_ticks / pit_frequency_hz) * 1000;

    if (duration_ms == 0) duration_ms = 1; // Avoid division by zero

    lapic_ticks_per_ms = ticks_elapsed / duration_ms;

    debugln("LAPIC timer calibrated: %u ticks/ms.", lapic_ticks_per_ms);

    // Stop the LAPIC timer by resetting initial count to 0 and disabling LVT.
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0);
    lapic_write(LAPIC_REG_LVT_TIMER, 0x10000); // Disable timer (bit 16)
}


void lapic_eoi() {
    // Assuming lapic_base is mapped correctly as we saw before
    *(volatile uint32_t*)((uintptr_t)lapic_base + LAPIC_REG_EOI) = 0;
}

// --- LAPIC Timer ISR ---
// Assembly wrapper for the LAPIC timer ISR.
// This function MUST be defined in an assembly file and linked.
// For now, declare it as extern.
extern void lapic_timer_isr_wrapper(void);

// C function for the LAPIC timer ISR
void lapic_timer_isr() {
    // This ISR is called when the LAPIC timer expires.

    // 1. Acknowledge the interrupt to the LAPIC.
    lapic_eoi();

    // For sleep, the LAPIC timer is in one-shot mode. Its expiration
    // is the signal that the sleep duration has ended. The 'hlt' instruction
    // will return when this interrupt occurs.
}

// --- Sleep Function using LAPIC Timer ---
// Implements power-saving sleep using the LAPIC timer in one-shot mode.
void sleep(uint32_t ms) {
    if (!lapic_base) {
        debugln("LAPIC not initialized, cannot sleep.");
        return;
    }
    if (lapic_ticks_per_ms == 0) {
        debugln("LAPIC timer not calibrated, cannot sleep accurately.");
        return;
    }

    // 1. Configure LAPIC Timer for One-Shot mode and set duration.
    //    Set the divider configuration.
    lapic_write(LAPIC_REG_DIVIDE_CONF, LAPIC_TIMER_DIVIDER_CONF);

    //    Set LVT Timer Register: One-Shot Mode (bit 17=0) | Delivery Vector.
    //    We use LAPIC_TIMER_VECTOR (defined as 32).
    uint32_t lvt_timer_value = LAPIC_TIMER_VECTOR; // One-shot, vector 32
    lapic_write(LAPIC_REG_LVT_TIMER, lvt_timer_value);

    //    Calculate and set the Initial Count.
    uint32_t ticks_to_wait = ms * lapic_ticks_per_ms;
    // Ensure at least one tick to avoid immediate expiration if ms is 0 or very small.
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    lapic_write(LAPIC_REG_INITIAL_COUNT, ticks_to_wait);

    // 2. Halt the CPU until the LAPIC timer interrupt occurs.
    //    This instruction puts the CPU into a low-power state.
    __asm__ volatile("hlt");

    // 3. When the LAPIC timer ISR fires, the CPU wakes up from HLT.
    //    The ISR sends LAPIC EOI, and control returns here. Sleep is complete.
}

void lapic_timer_test() {
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x03); 
    // Vector 32 | Periodic Mode
    lapic_write(LAPIC_REG_LVT_TIMER, 32 | (1 << 17)); 
    // Set a value. If this is 1,000,000, it ticks every million bus cycles.
    lapic_write(LAPIC_REG_INITIAL_COUNT, 1000000); 
}

