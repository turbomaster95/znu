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
#include <elf.h>

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
    // For SYSCALL from Ring 3, the CPU will 'swapgs' to pull this address 
    // into the active slot. So we store it in KERNEL_GS_BASE for now.
    
    wrmsr(MSR_GS_BASE, (uint32_t)addr, (uint32_t)(addr >> 32));
    wrmsr(MSR_KERNEL_GS_BASE, 0, 0); 

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
    // debugln("[sys_write] PID: %d, FD: %d, Buf: %p, Count: %d", current_process->pid, fd, buf, count);
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

int sys_spawn(const char* path) {
    if (!path || !is_user_addr((void*)path, 1)) return -1;

    char kpath[256];
    size_t i;
    for (i = 0; i < 255; i++) {
        if (!is_user_addr((void*)&path[i], 1)) return -1;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
    }
    kpath[i] = '\0';

    vfs_node_t* node = vfs_path_to_node(kpath);
    if (!node || node->type != VFS_FILE) return -1;

    process_t* proc = create_process_from_elf((uint8_t*)node->data);
    if (!proc) return -1;

    add_process(proc);
    return (int)proc->pid;
}

long sys_brk(void* addr) {
    if (!current_process) return -1;
    if (!addr) return (long)current_process->brk;

    uintptr_t new_brk = (uintptr_t)addr;
    if (new_brk < current_process->brk_start) return -1;

    if (new_brk > current_process->brk) {
        uintptr_t start = PAGE_ALIGN_UP(current_process->brk);
        uintptr_t end = PAGE_ALIGN_UP(new_brk);
        for (uintptr_t p = start; p < end; p += 4096) {
            void* phys = palloc_zero();
            map_page(current_process->pml4, p, (uint64_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        }
    }
    current_process->brk = new_brk;
    return (long)current_process->brk;
}

void* sys_mmap(void* addr, size_t len, int prot, int flags, int fd, uint64_t offset) {
    if (!current_process) return (void*)-1;
    
    // Minimal implementation: only anonymous private mappings supported for now
    if (!(flags & 0x20)) { // MAP_ANONYMOUS is 0x20 on Linux
        return (void*)-1;
    }

    // Use a fixed high address area for mmaps if addr is NULL
    static uintptr_t mmap_bump = 0x0000600000000000;
    if (!addr) {
        addr = (void*)mmap_bump;
        mmap_bump += PAGE_ALIGN_UP(len);
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + len;
    for (uintptr_t p = PAGE_ALIGN_DOWN(start); p < end; p += 4096) {
        void* phys = palloc_zero();
        map_page(current_process->pml4, p, (uint64_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    return addr;
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
        case 5: // fstat (stub)
            return 0;
        case 9: // mmap
            return (uint64_t)sys_mmap((void*)arg1, (size_t)arg2, (int)arg3, (int)arg4, (int)arg5, 0); // simplified
        case 12: // brk
            return (uint64_t)sys_brk((void*)arg1);
        case 59: // spawn (execve-like)
            return (uint64_t)sys_spawn((const char*)arg1);

        case 217: // getdents64
            return (uint64_t)sys_getdents((int)arg1, (void*)arg2, (size_t)arg3);

        case 99: // sysinfo
            return (uint64_t)sys_sysinfo((struct sysinfo*)arg1);

        case 39: // getpid
            if (current_process) return current_process->pid;
            return 0;

        case 60: // exit
            if (current_process) {
                extern void do_exit(int code);
                do_exit((int)arg1);
            }
            return 0;

        case 61: // wait
            if (current_process) {
                extern int do_wait(int pid);
                return (uint64_t)do_wait((int)arg1);
            }
            return -1;

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

