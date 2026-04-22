#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdlib.h>
#include <stdio.h>
#include <kernel/tty.h>
#include <idt.h>
#include <lapic.h>
#include <pi.h>
#include <page.h>

extern void draw_kernel_gui(void);
extern void gdt_init(void);
extern void lapic_timer_test(void);
extern void init_acpi(void);
extern void lapic_timer_test(void);
extern void gdt_reload_segments(void);
extern uint32_t lapic_ticks_per_ms;

// Set the base revision to 5, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

// Define the global variable that mappage.c is looking for
uint64_t hhdm_offset = 0;

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
volatile void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (hhdm_request.response) {
        hhdm_offset = hhdm_request.response->offset;
    }


    if (rsdp_request.response) {
        debugln("Got RSDP from Limine!");
    }

    // 2. Initialize your PMM (Bump Allocator) using the memmap
    if (memmap_request.response != NULL) {
        init_pmm(memmap_request.response);
    } else {
        debugln("CRITICAL: Memmap request response is NULL!");
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        #ifdef CONFIG_FB_TEST
         fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
        #endif
    }

    terminal_initialize();
    printf("Hello Kernel World!\n");
    printf("By Deva\n");
    debugln("Hello");


    #ifdef CONFIG_EG_GUI
     draw_kernel_gui();
    #endif

    uint16_t cs_reg;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs_reg));
    debugln("Current CS: 0x%x", cs_reg);

    gdt_init();
    gdt_reload_segments();
    debugln("GDT initialized");

    debugln("CS after GDT Init: 0x%x", cs_reg);

    idt_init();
    debugln("IDT initialized.");

    debugln("HHDM Offset: %p\n", hhdm_request.response->offset);
    debugln("RSDP Address: %p", rsdp_request.response->address);

    debugln("About to map page");
    uint64_t* pml4 = (uint64_t*)(read_cr3() + hhdm_offset);
    debugln("PML4[511] is: %p", pml4[511]);
    map_page(pml4, 0xffff8000fee00000, 0xfee00000, PTE_WRITABLE | PTE_CACHE_DISABLE);
    debugln("Mapped page!");

    // Initialize LAPIC first
    lapic_init();
    debugln("LAPIC initialized.");

    // Initialize PIT (or LAPIC timer later)
    pit_init(1000); // Use PIT for now, will switch to LAPIC timer
    debugln("PIT initialized for calibration.");

    __asm__ volatile("sti");
    debugln("Interupts Enabled.");

    calibrate_lapic_timer();
    debugln("LAPIC calibrated: %u ticks/ms", lapic_ticks_per_ms);

    debugln("Testing sleep(2000)...");
    uint64_t s_start = timer_ticks;
    sleep(2000);
    uint64_t s_end = timer_ticks;
    debugln("sleep(2000) finished. PIT ticks elapsed: %d", s_end - s_start);

    init_acpi();

    debug_ram_map(memmap_request.response);

    hcf(); // Halt
}
