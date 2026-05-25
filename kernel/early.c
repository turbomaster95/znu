#include <rtc.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdlib.h>
#include <kernel/display.h>
#include <kernel/tty.h>
#include <idt.h>
#include <lapic.h>
#include <symbols.h>
#include <pi.h>
#include <page.h>
#include <uacpi/uacpi.h>
#include <timekeeper.h>
#include <elf.h>
#include <syscall.h>
#include <gdt.h>
#include <proc.h>
#include <vfs.h>
#include <x86.h>
#include <pci.h>
#include <ahci.h>
#include <fat32.h>
#include <disk.h>
#include <cpuid.h>
#include <smp.h>
#include <kernel.h>
#include <sync.h>
#include <net.h>

spinlock_t terminal_print_lock = SPINLOCK_INIT;

bool krnl_init_done = false;

extern void kmain(void);

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_efi_system_table_request efi_st_request = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST_ID,
    .revision = 0,
    .response = NULL
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
    .response = NULL,
    .flags = 0 // Set to 1 for x2APIC mode
};

uint64_t hhdm_offset = 0;
uint64_t rsdp_addr = 0;

bool running_efi;
extern uintptr_t __stack_chk_guard;

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

volatile void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

void force_sync(void* addr) {
    volatile uint32_t* fb = (volatile uint32_t*)addr;
    uint32_t dummy = *fb; // Force a read
    (void)dummy;
}

void test_web_request(void) {
    uint8_t ip[4] = {1, 1, 1, 1};

    // 2. Connect via TCP to Port 80
    int id = tcp_connect(ip, 80);
    if (id < 0) {
        debugln("TCP Connect failed", 3, 1);
        return;
    }

    // 3. Send raw HTTP GET
    const char *req = "GET / HTTP/1.1\r\nHost: google.com\r\nConnection: close\r\n\r\n";
    tcp_send(id, req, strlen(req));

    // 4. Read response
    uint8_t response[1024];
    int len = tcp_recv(id, response, 1023);
    if (len > 0) {
        response[len] = 0;
        debugln("Received: %s", 1, 0, response);
    }

    tcp_close(id);
}

void simd_init(void) {
    if (!cpu_has(SSE_SUPPORT)) {
	debugln("[simd] Basic SSE Support is unavailable on this cpu!");
	debugln("[simd] Some apps might not run properly or crash the system..");
        return; 
    }

    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Clear EM
    cr0 |= (1 << 1);  // Set MP
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // Set OSFXSR
    cr4 |= (1 << 10); // Set OSXMMEXCPT
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
    debugln("[simd] Enabled Basic SSE/FPU Operations.");

    if (cpu_has(XSAVE_SUPPORT)) {        
        cr4 |= (1 << 18); // Set OSXSAVE
        asm volatile("mov %0, %%cr4" : : "r"(cr4));

        uint32_t xcr0_eax = (1 << 0) | (1 << 1); // Always enable x87 and SSE
        
        if (cpu_has(AVX_SUPPORT)) {
            xcr0_eax |= (1 << 2); // Enable AVX state
        }

        uint32_t edx = 0;
        asm volatile("xsetbv" : : "a"(xcr0_eax), "d"(edx), "c"(0));
	debugln("[simd] Enabled XSAVE!");
    }
}

uint64_t* kernel_pml4;
struct limine_rsdp_response *rsdp_response = NULL;
struct limine_module_response *mod_res = NULL;

volatile bool screen_lock = false; 

void lock_screen() { screen_lock = true; }
void unlock_screen() { screen_lock = false; }

void earlykmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    disable_smap();
    debugln("Disabled SMAP");

    debugln("[rng] About to seed!");
    seed_from_hardware();
    debugln("[rng] Seeded RNG with hardware noise!");

    __stack_chk_guard = (uintptr_t)rand() << 32 | rand();

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
        debugerr("Memmap request response is NULL!");
        hcf();
    }

    simd_init();

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    if (module_request.response == NULL || module_request.response->module_count == 0) {
        debugerr("No modules found! Did you add any initrd's to limine.conf?");
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    pat_init();

    terminal_initialize();
    vmm_ready = true;

    debugln("Test Log for KVM and others.");
    force_sync(framebuffer->address);

    // Shows the current cmdline from Limine
    if (cmdline_request.response == NULL || 
        cmdline_request.response->cmdline == NULL || 
        cmdline_request.response->cmdline[0] == '\0') {
        debugln("[kernel] No cmdline was passed.");
    } else {
        debugln("[kernel] Got cmdline!");
        debugln("[kernel] cmdline: %s", cmdline_request.response->cmdline);
        parse_cmdline(cmdline_request.response->cmdline);
    }
    if (efi_st_request.response == NULL) {
        debugln("[kernel] Not running on an EFI system.");
        running_efi = false;
    } else {
        debugln("[kernel] Running on an EFI system!");
        running_efi = true;
    }

    debugln("[kernel] Welcome to znu!");
    debugln("[tty] \033[1mGot a \033[1;31mC\033[1;33mO\033[1;32mL\033[1;34mO\033[1;35mR\033[0;1m Display?\033[0m");

    #ifdef CONFIG_EG_GUI
     draw_kernel_gui();
    #endif

    uint16_t cs_reg;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs_reg));
    debugln("[kernel] Current CS: 0x%x", cs_reg);

    __asm__ volatile("cli");
    gdt_init_core(0);
    debugln("[gdt] GDT initialized");

    __asm__ volatile("mov %%cs, %0" : "=r"(cs_reg));
    debugln("[kernel] CS after GDT Init: 0x%x", cs_reg);

    idt_global_init();
    idt_local_load();
    debugln("[idt] IDT initialized.");

    symbols_init();

    // Initialize PIT
    pit_init(1000);
    debugln("[pit] PIT initialized for calibration.");

    init_scheduler();
    debugln("[sched] Initialized Scheduler.");

    debugln("[kernel] HHDM Offset: %p", hhdm_request.response->offset);
    debugln("[kernel] RSDP Address: %p", rsdp_request.response->address);

    debugln("[kernel] About to map page");
    debugln("[kernel_debug] PML4[511] is: %p", kernel_pml4[511]);
    map_page(kernel_pml4, 0xffff8000fee00000, 0xfee00000, PTE_WRITABLE | PTE_CACHE_DISABLE);
    debugln("[SUCCESS] Mapped page!");

    rsdp_response = rsdp_request.response;
    mod_res = module_request.response;

    debugln("[early] Early Kernel Boot DONE!");
    kmain();

    hcf(); // Halt
}
