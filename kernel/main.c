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
#include <uacpi/uacpi.h>
#include <timekeeper.h>
#include <elf.h>
#include <syscall.h>
#include <gdt.h>

extern uacpi_status init_acpi(void);
extern void draw_kernel_gui(void);
extern void kernel_reboot(void);
extern void lapic_timer_test(void);
extern void lapic_timer_test(void);
extern uint32_t lapic_ticks_per_ms;
extern void jump_to_usermode(uintptr_t entry, uintptr_t stack);
extern uintptr_t stack_top;

void user_function() {
    while (1) {
     printf("h");
    }
}
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

__attribute__((section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

// Define the global variable that mappage.c is looking for
uint64_t hhdm_offset = 0;
uint64_t rsdp_addr = 0;

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

uint64_t* kernel_pml4;
struct limine_rsdp_response *rsdp_response = NULL;

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
        debugln("[kernel] Got RSDP from Limine!");
        rsdp_addr = (uintptr_t)rsdp_request.response->address;
    }

    if (memmap_request.response != NULL) {
        init_pmm(memmap_request.response);
        // Initializing VMM :)
        init_vmm(memmap_request.response);
        kernel_pml4 = vmm_get_kernel_pml4();
        init_slab();
    } else {
        debugln("CRITICAL: Memmap request response is NULL!");
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    if (module_request.response == NULL || module_request.response->module_count == 0) {
        debugln("Error: No modules found! Did you add init.elf to limine.conf?");
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
    debugln("[kernel] Welcome to znu!");


    #ifdef CONFIG_EG_GUI
     draw_kernel_gui();
    #endif

    uint16_t cs_reg;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs_reg));
    debugln("[kernel] Current CS: 0x%x", cs_reg);

    __asm__ volatile("cli");
    gdt_init();
    gdt_reload_segments();
    debugln("[gdt] GDT initialized");

    debugln("[kernel] CS after GDT Init: 0x%x", cs_reg);

    idt_init();
    debugln("[idt] IDT initialized.");

    debugln("[kernel] HHDM Offset: %p", hhdm_request.response->offset);
    debugln("[kernel] RSDP Address: %p", rsdp_request.response->address);

    debugln("[kernel] About to map page");
    debugln("[kernel_debug] PML4[511] is: %p", kernel_pml4[511]);
    map_page(kernel_pml4, 0xffff8000fee00000, 0xfee00000, PTE_WRITABLE | PTE_CACHE_DISABLE);
    debugln("[SUCCESS] Mapped page!");

    // Initialize LAPIC first
    lapic_init();
    debugln("[lapic] LAPIC initialized.");

    // Initialize PIT (or LAPIC timer later)
    pit_init(1000); // Use PIT for now, will switch to LAPIC timer
    debugln("[pit] PIT initialized for calibration.");

    __asm__ volatile("sti");
    debugln("[kernel] Interupts Enabled.");

    calibrate_lapic_timer();
    debugln("[lapic] LAPIC calibrated: %u ticks/ms", lapic_ticks_per_ms);

    debugln("[ktest] Testing sleep(1000)...");
    uint64_t s_start = timer_ticks;
    sleep(1000);
    uint64_t s_end = timer_ticks;
    debugln("[ktest] sleep(1000) finished. PIT ticks elapsed: %d", s_end - s_start);

    rsdp_response = rsdp_request.response;

    debugln("[kernel] Basic System Initialization done!");
    debugln("[kernel] Starting uACPI...");

    // Stage 1: Table initialization
    uacpi_status status = uacpi_initialize(UACPI_LOG_DEBUG);
    if (status != UACPI_STATUS_OK) {
        debugerr("[ERROR] uACPI init failed: %s", uacpi_status_to_string(status));
        hcf();
    }
    debugln("[kernel] uACPI Initialized!");

    // Stage 2: Load the AML namespace
    status = uacpi_namespace_load();
    if (status != UACPI_STATUS_OK) {
        debugerr("[ERROR] Namespace load failed!");
    }
    debugln("[kernel] uACPI Namespace Loaded!");

    // Stage 3: Initialize devices
    debugln("[kernel_debug] About to initialize namespace..");
    status = uacpi_namespace_initialize();
    debugln("[SUCCESS] uACPI is live.");

    enable_syscalls();
    syscall_init();
    gs_init(stack_top);
    debugln("[kernel] Jumping to Ring 3...");
    struct limine_file *init_file = module_request.response->modules[0];
    load_elf(init_file->address);

    abort();
    hcf(); // Halt
}
