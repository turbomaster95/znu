#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limine.h>
#include <stdio.h>
#include <pi.h>
#include <tsc.h>
#include <idt.h>
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

static bool g_x2apic_mode = false;

PERFORM uint32_t lapic_read(uint32_t offset) {
    return *(volatile uint32_t*)((uintptr_t)lapic_base + offset);
}

PERFORM void lapic_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((uintptr_t)lapic_base + offset) = value;
}

void lapic_init_per_core(void) {
    if (lapic_base == NULL) return;

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
    lo &= ~(1 << 10);   // Force disable x2APIC
    lo |= (1 << 11);    // Enable APIC base mapping
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(0x1B));

    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    lapic_write(LAPIC_REG_SVR, svr | 0x100 | 0xFF);

    lapic_write(0x350, 0x700);

    if (lapic_ticks_per_ms > 0) {
        lapic_write(LAPIC_REG_DIVIDE_CONF, 0x03); 
        // Set vector with periodic mode enabled (Bit 17)
        lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | (1 << 17)); 
        lapic_write(LAPIC_REG_INITIAL_COUNT, lapic_ticks_per_ms);
    }
//    lapic_write(LAPIC_REG_TPR, 0);
}

void lapic_init() {
    if (lapic_base != NULL) return;

    if (!hhdm_request.response) {
        debugerr("HHDM NOT FOUND");
        return; 
    }

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));

    uintptr_t lapic_phys = (lo & 0xFFFFF000) | ((uint64_t)(hi & 0x0F) << 32);
    lapic_base = (volatile uint64_t*)(lapic_phys + hhdm_request.response->offset);

    debugln("[dlapic] LAPIC Base (Phys): 0x%p", lapic_phys);
    debugln("[dlapic] LAPIC Base (Virt): 0x%p", lapic_base);

    // Run local hardware initialization for the BSP
    lapic_init_per_core();
}

int get_cpu_id(void) {
    if (lapic_base == NULL) return 0;
    
    // LAPIC ID Register is at offset 0x20 bytes
    volatile uint32_t* lapic_id_reg = (volatile uint32_t*)((uintptr_t)lapic_base + 0x20);
    uint32_t lapic_id = (*lapic_id_reg) >> 24;

    return (int)lapic_id;
}

PERFORM void calibrate_lapic_timer_no_irq() {
    debugln("[clapic] Calibrating LAPIC timer using TSC...");

    extern tsc_info_t tsc_data;

    if (!tsc_data.supported || tsc_data.frequency == 0) {
        debugwarn("[clapic] TSC unavailable, falling back to PIT ch2 polling.");
        outb(0x61, (inb(0x61) & ~0x02) | 0x01);
        outb(0x43, 0xB0);
        outb(0x42, 0xFF);
        outb(0x42, 0xFF);

        lapic_write(LAPIC_REG_DIVIDE_CONF,   0x03);
        lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

        while (!(inb(0x61) & 0x20));

        uint32_t lapic_remaining = lapic_read(LAPIC_REG_CURRENT_COUNT);
        lapic_write(LAPIC_REG_INITIAL_COUNT, 0);
        outb(0x61, inb(0x61) & ~0x01);

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
    if (!lapic_base && !g_x2apic_mode) {
        debugerr("LAPIC not initialized, cannot calibrate timer.");
        return;
    }
    if (lapic_ticks_per_ms != 0) {
        debugln("[lapic] LAPIC timer already calibrated.");
        return;
    }

    debugln("[clapic] Calibrating LAPIC timer...");

    uint64_t pit_start_ticks = timer_ticks;
    uint64_t pit_wait_duration_ticks = 100;

    lapic_write(LAPIC_REG_DIVIDE_CONF, LAPIC_TIMER_DIVIDER_CONF);
    uint32_t lvt_timer_value = (32);
    lapic_write(LAPIC_REG_LVT_TIMER, lvt_timer_value);
    lapic_write(LAPIC_REG_INITIAL_COUNT, 0xFFFFFFFF);

    uint32_t lapic_start_count = lapic_read(LAPIC_REG_CURRENT_COUNT);

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

    uint32_t pit_frequency_hz = 1000;
    uint32_t duration_ms = (pit_wait_duration_ticks * 1000) / pit_frequency_hz;

    if (duration_ms == 0) duration_ms = 1;

    lapic_ticks_per_ms = ticks_elapsed / duration_ms;

    debugln("[clapic] APIC timer calibrated: %u ticks/ms.", lapic_ticks_per_ms);

    lapic_write(LAPIC_REG_INITIAL_COUNT, 0);
    lapic_write(LAPIC_REG_LVT_TIMER, 0x10000);
}

void lapic_eoi() {
    if (g_x2apic_mode) {
        __asm__ volatile("wrmsr" : : "a"(0), "d"(0), "c"(0x80B));
        return;
    }
    if (lapic_base) {
        *(volatile uint32_t*)((uintptr_t)lapic_base + LAPIC_REG_EOI) = 0;
    }
}

extern void lapic_timer_isr_wrapper(void);

PERFORM void lapic_timer_isr() {
    lapic_eoi();
}

PERFORM void sleep(uint32_t ms) {
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

void lapic_broadcast_panic_nmi(void) {
    while (lapic_read(LAPIC_REG_ICR_LOW) & (1 << 12)) {
        __asm__ volatile("pause");
    }

    lapic_write(LAPIC_REG_ICR_HIGH, 0);

    uint32_t icr_mask = (3 << 18) | (1 << 14) | (4 << 8);

    lapic_write(LAPIC_REG_ICR_LOW, icr_mask);
}

void lapic_send_ipi(uint8_t lapic_id, uint8_t vector) {
    while (lapic_read(LAPIC_REG_ICR_LOW) & (1 << 12)) {
        __asm__ volatile("pause");
    }

    lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)lapic_id << 24);

    uint32_t icr_low = (vector & 0xFF); 
    
    lapic_write(LAPIC_REG_ICR_LOW, icr_low);
}
