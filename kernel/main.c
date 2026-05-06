#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdlib.h>
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
#include <proc.h>
#include <vfs.h>
#include <x86.h>
#include <kernel.h>

bool krnl_init_done = false;
extern void serial_init();

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

// Define the global variable that mappage.c is looking for
uint64_t hhdm_offset = 0;
uint64_t rsdp_addr = 0;

bool running_efi;
extern uintptr_t __stack_chk_guard;

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

void sse_init(void) {
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
}

uint64_t* kernel_pml4;
struct limine_rsdp_response *rsdp_response = NULL;

volatile bool screen_lock = false; 

// This prevents flanterm from taking over the screen if graphics are to be drawn
void lock_screen() { screen_lock = true; }
void unlock_screen() { screen_lock = false; }

void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    disable_smap();
    debugln("Disabled SMAP");
    // This starts the random number generator
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

    // Does some neat shit with the init fb
    int padding = 6;
    uint32_t screen_w = framebuffer->width;
    uint32_t screen_h = framebuffer->height;

    TERM_W = screen_w - (padding * 2);
    TERM_H = screen_h - (padding * 2);
    term_x = padding;
    term_y = padding;

    // 3. Draw the "thick" screen border at the extreme edges
    uint32_t border_color = 0xFF2F334D;
    for (int i = 0; i < 6; i++) {
      draw_outline_rect(i, i, screen_w - (i * 2), screen_h - (i * 2), border_color);
    }

    terminal_initialize();
    blit_window(term_x, term_y, TERM_W, TERM_H, term_buffer);
    vmm_ready = true;

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
    gdt_init();
    gdt_reload_segments();
    debugln("[gdt] GDT initialized");

    debugln("[kernel] CS after GDT Init: 0x%x", cs_reg);

    idt_init();
    debugln("[idt] IDT initialized.");
    sse_init();
    debugln("[cpu] SSE initialized.");

    debugln("[kernel] HHDM Offset: %p", hhdm_request.response->offset);
    debugln("[kernel] RSDP Address: %p", rsdp_request.response->address);

    debugln("[kernel] About to map page");
    debugln("[kernel_debug] PML4[511] is: %p", kernel_pml4[511]);
    map_page(kernel_pml4, 0xffff8000fee00000, 0xfee00000, PTE_WRITABLE | PTE_CACHE_DISABLE);
    debugln("[SUCCESS] Mapped page!");

    // Initialize LAPIC first
    lapic_init();
    debugln("[lapic] LAPIC initialized.");

    // Initialize PIT
    pit_init(1000);
    debugln("[pit] PIT initialized for calibration.");

   
    serial_init();
    debugln("[serial] Serial initialized.");
    ps2_init();
    debugln("[ps2] Initialized PS/2");

    __asm__ volatile("sti");
    debugln("[kernel] Interupts Enabled.");

    calibrate_lapic_timer();
    debugln("[lapic] LAPIC calibrated: %u ticks/ms", lapic_ticks_per_ms);

    debugln("[ktest] Testing sleep(2000)...");
    uint64_t s_start = timer_ticks;
    sleep(2000);
    uint64_t s_end = timer_ticks;
    debugln("[ktest] sleep(2000) finished. PIT ticks elapsed: %d", s_end - s_start);
    if (s_end - s_start != 2000) {
        debugwarn("sleep(2000) is not accurate!");
        uint64_t elapsed = s_end - s_start;
        uint64_t error = (elapsed > 2000) ? (elapsed - 2000) : (2000 - elapsed);
        debugln("[ktest] Thats like.. %d of error!", (uint32_t)error);
    } else {
        debugln("[ktest] sleep(2000) is accurate!");
    }
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

    init_vfs();

    struct limine_module_response *mod_res = module_request.response;
    if (mod_res == NULL || mod_res->module_count == 0) {
       panic("Initramfs (CPIO) module not found! Check limine.conf");
    }

    void* initrd_addr = mod_res->modules[0]->address;
    size_t initrd_size = mod_res->modules[0]->size;
    
    // Check for LZ4 magic number (Standard: 0x184D2204, Legacy: 0x184C2102)
    uint32_t magic = *(uint32_t*)initrd_addr;
    debugln("[kernel] Initramfs magic: 0x%x", magic);
    if (magic == 0x184D2204 || magic == 0x184C2102) {
        debugln("[kernel] Detected LZ4 compressed initramfs (%s)", 
                magic == 0x184D2204 ? "Standard" : "Legacy");
        // Allocate a buffer for decompression (64MB)
        size_t decomp_size_max = 64 * 1024 * 1024;
        void* decomp_buffer = kmalloc(decomp_size_max);
        if (!decomp_buffer) {
            panic("Failed to allocate buffer for initramfs decompression!");
        }
        
        int actual_size = lz4_unframe(initrd_addr, decomp_buffer, initrd_size, decomp_size_max);
        if (actual_size < 0) {
            debugerr("LZ4 decompression failed with code %d", actual_size);
            panic("Initramfs decompression failed!");
        }
        
        debugln("[kernel] Decompressed initramfs: %d bytes", actual_size);
        initrd_addr = decomp_buffer;
    }

    debugln("\033[1;34m  ______             \033[0m\n");
    debugln("\033[1;34m |___  /             \033[0m\n");
    debugln("\033[1;34m    / / _ __  _   _  \033[0m\n");
    debugln("\033[1;34m   / / | '_ \\| | | | \033[0m\n");
    debugln("\033[1;34m  / /__| | | | |_| | \033[0m\n");
    debugln("\033[1;34m /_____|_| |_|\\__,_| \033[0m\n");
    debugln("\033[1;36m  Kernel v0.1.0-alpha \033[0m\n\n");

    debugln("[kernel] Parsing initramfs at %p...", initrd_addr);
    cpio_parse(initrd_addr);

    vfs_node_t* init_node = vfs_path_to_node("/bin/init");
    if (!init_node) {
       panic("Initramfs parsed, but /bin/init not found in VFS!");
    }
    
    debugln("[kernel] Loading init process from VFS (Size: %d bytes)", init_node->size);
    init_scheduler();
    debugln("[sched] Initialized Scheduler.");

    const char* init_argv[] = {"/bin/init", NULL};
    const char* init_envp[] = {"PATH=/bin", "TERM=linux", "HOME=/", NULL};
    process_t* init_proc = create_process_from_elf((uint8_t*)init_node->data, (char**)init_argv, (char**)init_envp);
    
    add_process(init_proc);
    init_proc->state = TASK_RUNNING;
    current_process = init_proc;
    init_process = init_proc;

    // Set the current process for the scheduler/syscalls
    extern process_t* current_process;
    current_process = init_process;

    if (init_proc) {
       vmm_switch(init_proc->pml4);
       
       extern struct tss kernel_tss;
       extern cpu_context_t main_cpu_context;
       
       kernel_tss.rsp0 = init_proc->kstack_top;
       main_cpu_context.kernel_stack = init_proc->kstack_top;

       debugln("[kernel] Jumping to Ring 3...");
       jump_to_usermode(init_proc->entry, init_proc->stack_top);
    }    

    panic("Init binary exited!!!");

    hcf(); // Halt
}
