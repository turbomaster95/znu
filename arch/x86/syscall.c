#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>
#include <stddef.h>
#include <string.h>
#include <proc.h>
#include <vfs.h>
#include <page.h>
#include <stdio.h>
#include <kernel/tty.h>

extern void hcf(void);
extern void syscall_entry(void);
extern void kernel_reboot(void);
extern void kernel_shutdown(void);

#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_SFMASK       0xC0000084
#define MSR_GS_BASE      0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102
#define EFER_MSR 0xC0000080
#define EFER_SCE (1 << 0) // System Call Enable

extern size_t keyboard_read(char* buf, size_t count);

static bool is_user_addr(void* ptr, size_t len) {
    uintptr_t addr = (uintptr_t)ptr;
    // Just check if it's in the lower half (Canonical address)
    return addr < 0x0000800000000000ULL;
}

void enable_syscalls() {
    uint32_t low, high;
    // Read current EFER value
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(EFER_MSR));

    // Set the SCE bit
    low |= EFER_SCE;

    // Write it back
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(EFER_MSR));
}

cpu_context_t main_cpu_context;

void gs_init(uintptr_t kernel_stack_top) {
    main_cpu_context.kernel_stack = kernel_stack_top;
    main_cpu_context.user_stack_scratch = 0;

    uintptr_t addr = (uintptr_t)&main_cpu_context;

    // IMPORTANT: In Ring 0 (now), GS_BASE is active.
    // But for SYSCALL from Ring 3, we need the address in KERNEL_GS_BASE
    // so that 'swapgs' can pull it into the active slot.
    
    wrmsr(MSR_KERNEL_GS_BASE, (uint32_t)addr, (uint32_t)(addr >> 32));
    
    // Set active GS to 0 for now, so we know swapgs actually does something later
    wrmsr(MSR_GS_BASE, 0, 0); 

    debugln("[sys] GS Shadow initialized to %p", (void*)addr);
}

void syscall_init() {
    uint32_t lo, hi;

    // STAR MSR Configuration
    // [63:48] User Segments: Base for SYSRET. 
    //         SYSRET sets SS = (Base + 8) and CS = (Base + 16)
    //         To get SS=0x18 and CS=0x20, we must set Base to 0x10.
    // [47:32] Kernel Segments: Base for SYSCALL.
    //         SYSCALL sets SS = (Base + 8) and CS = (Base)
    //         To get CS=0x08 and SS=0x10, we set Base to 0x08.
    
    uint32_t kernel_base = 0x08;
    uint32_t user_base = 0x10; // (0x10 + 8 = 0x18 for SS, 0x10 + 16 = 0x20 for CS)

    hi = (user_base << 16) | kernel_base;
    lo = 0;
    wrmsr(MSR_STAR, lo, hi);

    // LSTAR: The 64-bit entry point
    uintptr_t entry = (uintptr_t)syscall_entry;
    wrmsr(MSR_LSTAR, (uint32_t)entry, (uint32_t)(entry >> 32));

    // SFMASK: Bits to clear in RFLAGS on entry.
    // Clear Interrupts (0x200), Trap (0x100), and Direction (0x400)
    wrmsr(MSR_SFMASK, 0x700, 0);

    debugln("[sys] Syscall MSRs initialized.");
}

long sys_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    if (!buf || !is_user_addr(buf, count)) return -1;
    if (!current_process) return -1;
    
//    if (fd == 0) debugln("[sys] sys_read(fd=%d, buf=%p, count=%d)", fd, buf, (int)count);
    
    if (fd < 3) {
        if (fd == 0) {
            size_t got = 0;
            char* user_buf = (char*)buf;
            while (got < count) {
                char c;
                if (keyboard_read(&c, 1) > 0) {
                    user_buf[got++] = c;
                    return got;
                }
                __asm__ volatile("sti; hlt; cli");
            }
            return (long)got;
        }
        return -1;
    }

    vfs_file_t* file = current_process->files[fd];
    if (!file) return -1;

    int ret = vfs_read(file->node, buf, count, file->pos);
    if (ret > 0) file->pos += ret;
    return (long)ret;
}

long sys_write(int fd, const void* buf, size_t count) {
    if (!current_process) return -1;
    if (!buf || !is_user_addr((void*)buf, count)) return -1;

    if (fd == 1 || fd == 2) {
        const char* ptr = (const char*)buf;
        for (size_t i = 0; i < count; i++) debug_putchar(ptr[i]);
        return count;
    }

    if (fd < 0 || fd >= MAX_FILES) {
        return -1;
    }
    vfs_file_t* file = current_process->files[fd];
    if (!file) {
        // Fallback for stdout/stderr markers or uninitialized files
        if (fd == 1 || fd == 2) {
            const char* ptr = (const char*)buf;
            for (size_t i = 0; i < count; i++) debug_putchar(ptr[i]);
            return count;
        }
        return -1;
    }

    int ret = vfs_write(file->node, buf, count, file->pos);
    if (ret > 0) file->pos += ret;
    return (long)ret;
}

int sys_open(const char* path, int flags) {
    if (!path || !is_user_addr((void*)path, 1)) return -1;
    if (!current_process) return -1;

    // Copy path to kernel buffer to avoid SMAP issues and ensure null termination
    char kpath[256];
    size_t i;
    for (i = 0; i < 255; i++) {
        if (!is_user_addr((void*)&path[i], 1)) return -1;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
    }
    kpath[i] = '\0';
 
    extern vfs_node_t* root_node;
    vfs_node_t* node = vfs_path_to_node(kpath);
    if (!node) return -1;

    if (!current_process) return -1;
    for (int i = 3; i < MAX_FILES; i++) {
        if (current_process->files[i] == NULL) {
            vfs_file_t* file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
            if (!file) return -1;
            
            file->node = node;
            file->pos = 0;
            file->flags = flags;
            current_process->files[i] = file;
            return i;
        }
    }
    return -1;
}

int sys_close(int fd) {
    if (fd < 3) return -1;
    if (fd < 0 || fd >= MAX_FILES) return -1;
    if (current_process->files[fd]) {
        kfree(current_process->files[fd]);
        current_process->files[fd] = NULL;
        return 0;
    }
    return -1;
}

typedef struct {
    char name[128];
    uint32_t type;
    uint32_t size;
} znu_dirent_t;

int sys_getdents(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    if (!buf || !is_user_addr(buf, count)) return -1;
    
    vfs_file_t* file = current_process->files[fd];
    if (!file || file->node->type != VFS_DIRECTORY) return -1;

    // Use a kernel buffer to avoid SMAP issues during population
    znu_dirent_t kdent;
    int read = 0;
    int skip = file->pos;
    
    vfs_node_t* child = file->node->children;
    for (int i = 0; i < skip && child; i++) {
        child = child->next;
    }

    uint8_t* user_ptr = (uint8_t*)buf;
    int count_found = 0;
    while (child) {
        count_found++;
        if (read + sizeof(znu_dirent_t) <= count) {
            memset(&kdent, 0, sizeof(kdent));
            strncpy(kdent.name, child->name, 127);
            kdent.type = child->type;
            kdent.size = child->size;
            
            // Perform copy
            memcpy(user_ptr + read, &kdent, sizeof(kdent));
            read += sizeof(znu_dirent_t);
            file->pos++;
        }
        child = child->next;
    }

    return read;
}

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[20-2*sizeof(long)-sizeof(int)];
};

extern uint64_t pmm_get_total_pages(void);
extern uint64_t pmm_get_free_pages(void);
extern int process_count;

int sys_sysinfo(struct sysinfo* info) {
    if (!info || !is_user_addr(info, sizeof(struct sysinfo))) return -1;
    
    struct sysinfo kinfo;
    memset(&kinfo, 0, sizeof(kinfo));
    
    kinfo.totalram = pmm_get_total_pages() * 4096;
    kinfo.freeram = pmm_get_free_pages() * 4096;
    kinfo.procs = (unsigned short)process_count;
    kinfo.mem_unit = 1;
    
    memcpy(info, &kinfo, sizeof(struct sysinfo));
    return 0;
}

uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (num) {
        case 0: // read
            return (uint64_t)sys_read((int)arg1, (void*)arg2, (size_t)arg3);
        case 1: // write
            return (uint64_t)sys_write((int)arg1, (const void*)arg2, (size_t)arg3);
        case 2: // open
            return (uint64_t)sys_open((const char*)arg1, (int)arg2);
        case 3: // close
            return (uint64_t)sys_close((int)arg1);

        case 217: // getdents64
            return (uint64_t)sys_getdents((int)arg1, (void*)arg2, (size_t)arg3);

        case 99: // sysinfo
            return (uint64_t)sys_sysinfo((struct sysinfo*)arg1);

        case 39: // getpid
            if (current_process) return current_process->pid;
            return 0;

        case 60: // exit
            if (current_process) {
                current_process->state = TASK_ZOMBIE;
                debugln("[proc] Process %d exited with code %d", current_process->pid, (int)arg1);
                while(1) {
                    asm volatile("sti; hlt");
                }
            }
            return 0;

        case 169: // reboot
            debugln("\n\nReboot called from user process\n\n");
	    kernel_reboot();
            return 0;
        
        case 48:
            debugln("\n\nShutdown called from user process\n\n");
            kernel_shutdown();
            return 0;

        default:
            return -1;
    }
}

