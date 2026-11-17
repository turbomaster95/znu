#include <rtc.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <stdlib.h>
#include <kernel/display.h>
#include <kernel/tty.h>
#include <kernel/module.h>
#include <kernel/initcall.h>
#include <symbols.h>
#include <page.h>
#include <uacpi/uacpi.h>
#include <timekeeper.h>
#include <elf.h>
#include <syscall.h>
#include <proc.h>
#include <vfs.h>
#include <vfse.h>
#include <fat32.h>
#include <disk.h>
#include <mp.h>
#include <misc.h>
#include <sync.h>
#include <net.h>
#include <e1000.h>
#include <generated/nifties.h>
#include <devkit/devkit.h>

extern void hcf(void);
extern struct limine_module_response *mod_res;
extern void test_smp_workers(int total_cores);
extern void net_test_suite(void);

void kmain(void) {
    p_info("znu", "Reached Kmain!");

    rtc_init();
    timekeeper_init();

    arch_main_init();

    #ifdef CONFIG_KTEST
     p_info("ktest", "Testing sleep(2000)...");
     uint64_t s_start = nifties;
     sleep(2000);
     uint64_t s_end = nifties;
     p_debug("ktest", "sleep(2000) finished. PIT ticks elapsed: %d", s_end - s_start);
     if (s_end - s_start != 2000) {
         debugwarn("sleep(2000) is not accurate!");
         uint64_t elapsed = s_end - s_start;
         uint64_t error = (elapsed > 2000) ? (elapsed - 2000) : (2000 - elapsed);
         p_warn("ktest", "Thats like.. %d of error!", (uint32_t)error);
     } else {
         p_info("ktest", "sleep(2000) is accurate!");
     }
    #endif

    p_info("kernel", "Basic System Initialization done!");

    arch_basic_init();

    devkit_init();

    do_initcalls();

    //  extern void terminal_teardown(void);
    //  terminal_teardown();

    #ifdef CONFIG_E1000
       extern pci_device_t *pci_dev;
       e1000_init();
       if (!pci_dev) {
           debugerr("[kernel] E1000 NIC not found or initialization failed!");
       } else {
           net_init();
       }
    #endif

    // sleep(1000);
    // net_test_suite();

    #ifdef CONFIG_SMP
    mp_init();

    sleep(10);
    test_smp_workers(2);

    #endif

    disk_init();

    init_vfs();
    p_info("vfs", "Initialized VFS");

    vfse_init();
    p_info("vfse", "Initialized VFSe");

    tty_init();
    p_info("tty", "Initialized tty");

    syscall_init();
    gs_init(stack_top);

    if (mod_res == NULL || mod_res->module_count == 0) {
       panic("Initramfs (CPIO) module not found! Check limine.conf");
    }

    void* initrd_addr = mod_res->modules[0]->address;
    size_t initrd_size = mod_res->modules[0]->size;
    
    // Check for LZ4 magic number (Standard: 0x184D2204, Legacy: 0x184C2102)
    uint32_t magic = *(uint32_t*)initrd_addr;
    p_debug("kernel", "Initramfs magic: 0x%x", magic);
    if (magic == 0x184D2204 || magic == 0x184C2102) {
        p_info("kernel", "Detected LZ4 compressed initramfs (%s)", 
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
        
        p_info("kernel", "Decompressed initramfs: %d bytes", actual_size);
        initrd_addr = decomp_buffer;
    }

    enable_smap();
    p_info("smap", "Reenabled SMAP");

    debugln("\033[1;34m  ______             \033[0m");
    debugln("\033[1;34m |___  /             \033[0m");
    debugln("\033[1;34m    / / _ __  _   _  \033[0m");
    debugln("\033[1;34m   / / | '_ \\| | | | \033[0m");
    debugln("\033[1;34m  / /__| | | | |_| | \033[0m");
    debugln("\033[1;34m /_____|_| |_|\\__,_| \033[0m");
    debugln("\033[1;36m  Kernel v0.1.0-alpha \033[0m\n");

    p_info("kernel", "Parsing initramfs at %p...", initrd_addr);
    cpio_parse(initrd_addr);

    vfs_node_t* init_node = vfs_path_to_node("/bin/init");
    if (!init_node) {
       panic("Initramfs parsed, but /bin/init not found in VFS!");
    }
    
    p_info("kernel", "Loading init process from VFS (Size: %d bytes)", init_node->size);
    
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

    vfs_node_t* init_module = vfs_path_to_node("/init.ko");

    if (!init_module) {
        p_warn("kernel", "init.ko initialization module not found!");
    } else {
        load_kernel_module("ModInit", (uint8_t*)init_module->data, init_module->size, NULL);
    }

    if (init_proc) {
       vmm_switch(init_proc->pml4);
       
       extern cpu_context_t main_cpu_context;
       
       //tss_per_cpu[0].rsp0 = init_proc->kstack_top;
       main_cpu_context.kernel_stack = init_proc->kstack_top;

       p_info("kernel", "Jumping to Ring 3...");
       jump_to_usermode(init_proc->entry, init_proc->stack_top);
       panic("Init binary exited!!!");
    }    

    panic("Init binary exited!!!");

    hcf(); // Halt
}
