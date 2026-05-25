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
#include <e1000.h>

extern void hcf(void);
extern struct limine_module_response *mod_res;

void kmain(void) {
    debugln("[znu] Reached Kmain!");

    lapic_init();
    debugln("[lapic] LAPIC initialized.");

    serial_init();
    debugln("[serial] Serial initialized.");
    ps2_init();

    rtc_init();
    timekeeper_init();
    debugln("[lapic] LAPIC calibrated: %u ticks/ms", lapic_ticks_per_ms);

    __asm__ volatile("cli");
    pit_init(1000); 
    outb(0x21, 0xFD); 
    __asm__ volatile("sti");
    
    debugln("[lapic] Starting LAPIC periodic timer..."); 
    lapic_write(LAPIC_REG_DIVIDE_CONF, 0x3);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | (1 << 17));
    lapic_write(LAPIC_REG_INITIAL_COUNT,lapic_ticks_per_ms);
    debugln("[lapic] LAPIC timer armed.");

    #ifdef CONFIG_KTEST
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
    #endif 

    debugln("[kernel] Basic System Initialization done!");
    debugln("[kernel] Starting uACPI...");

    uacpi_status status = uacpi_initialize(UACPI_LOG_DEBUG);
    if (status != UACPI_STATUS_OK) {
        debugerr("[ERROR] uACPI init failed: %s", uacpi_status_to_string(status));
        hcf();
    }
    //debugln("[kernel] uACPI Initialized!");

    status = uacpi_namespace_load();
    if (status != UACPI_STATUS_OK) {
        debugerr("[ERROR] Namespace load failed!");
    }
    //debugln("[kernel] uACPI Namespace Loaded!");

    //debugln("[kernel_debug] About to initialize namespace..");
    status = uacpi_namespace_initialize();
    debugln("[SUCCESS] uACPI is live.");

    pci_init();
    debugln("[pci] Init done!");

    ahci_init();
    debugln("[ahci] Init done!");
    
    disk_init();

    init_vfs();
    debugln("[vfs] Initialized VFS");

    tty_init();
    debugln("[tty] Initialized tty");


    #ifdef CONFIG_E1000
       e1000_init();
    #endif

    net_init();

    sleep(1000);
    //test_web_request();

    smp_init();
    debugln("[smp] ALL CORES HAVE BEEN WAKEN UP!!!!");

    enable_syscalls();
    syscall_init();
    gs_init(stack_top);

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

    enable_smap();
    debugln("[smap] Reenabled SMAP");

    #ifdef CONFIG_BGA
      extern void terminal_teardown(void);
      terminal_teardown();
      extern void bga_init(void);
      bga_init();
    #endif 

    debugln("\033[1;34m  ______             \033[0m");
    debugln("\033[1;34m |___  /             \033[0m");
    debugln("\033[1;34m    / / _ __  _   _  \033[0m");
    debugln("\033[1;34m   / / | '_ \\| | | | \033[0m");
    debugln("\033[1;34m  / /__| | | | |_| | \033[0m");
    debugln("\033[1;34m /_____|_| |_|\\__,_| \033[0m");
    debugln("\033[1;36m  Kernel v0.1.0-alpha \033[0m\n");

    debugln("[kernel] Parsing initramfs at %p...", initrd_addr);
    cpio_parse(initrd_addr);

    vfs_node_t* init_node = vfs_path_to_node("/bin/init");
    if (!init_node) {
       panic("Initramfs parsed, but /bin/init not found in VFS!");
    }
    
    debugln("[kernel] Loading init process from VFS (Size: %d bytes)", init_node->size);
    
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
       
       extern cpu_context_t main_cpu_context;
       
       tss_per_cpu[0].rsp0 = init_proc->kstack_top;
       main_cpu_context.kernel_stack = init_proc->kstack_top;

       debugln("[kernel] Jumping to Ring 3...");
       jump_to_usermode(init_proc->entry, init_proc->stack_top);
       panic("Init binary exited!!!");
    }    

    panic("Init binary exited!!!");

    hcf(); // Halt
}
