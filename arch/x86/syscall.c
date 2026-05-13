#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>
#include <stddef.h>
#include <string.h>
#include <proc.h>
#include <vfs.h>
#include <page.h>
#include <stdio.h>
#include <kernel/display.h>
#include <kernel/tty.h>
#include <elf.h>
#include <fcntl.h>

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

#include <sys/stat.h>

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
    
    vfs_file_t* file = current_process->files[fd];

    if (!file) return -1;

    int ret = file->node->ops->read(
  	  file->node,
  	  buf,
  	  count,
  	  file->pos
    );
    if (ret > 0) file->pos += ret;
    return (long)ret;
}

long sys_write(int fd, const void* buf, size_t count) {
    if (!current_process) return -1;

    if (fd < 0 || fd >= MAX_FILES)
        return -1;

    if (!buf || !is_user_addr((void*)buf, count))
        return -1;

    vfs_file_t* file = current_process->files[fd];

    if (!file) return -1;

    if (!(file->flags & O_WRONLY) &&
        !(file->flags & O_RDWR))
        return -1;

    int ret = file->node->ops->write(
  	  file->node,
  	  buf,
  	  count,
  	  file->pos
    );

    if (ret > 0)
        file->pos += ret;

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

int sys_getdents(int fd, void* buf, size_t count) {
    vfs_file_t* file = current_process->files[fd];
    if (!file || file->node->type != VFS_DIRECTORY) return -1;

    // BRIDGE: If this is a FAT32/Disk mount, use the driver's readdir
    if (file->node->is_mountpoint && file->node->ops->readdir) {
        int bytes = file->node->ops->readdir(file->node, file->pos, buf, count);
        if (bytes > 0) {
            // Increment position by number of entries read
            file->pos += (bytes / sizeof(znu_dirent_t));
        }
        return bytes;
    }

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

int sys_stat(const char* path, struct stat* buf) {
    if (!path || !is_user_addr((void*)path, 1)) return -1;
    if (!buf || !is_user_addr(buf, sizeof(struct stat))) return -1;

    char kpath[256];
    size_t i;
    for (i = 0; i < 255; i++) {
        if (!is_user_addr((void*)&path[i], 1)) return -1;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
    }
    kpath[i] = '\0';

    debugln("[sys] stat path: %s", kpath);    vfs_node_t* node = vfs_path_to_node(kpath);
    debugln("[sys] stat: %s", kpath);    if (!node) return -1;

    struct stat kstat;
    memset(&kstat, 0, sizeof(kstat));
    kstat.st_size = node->size;
    kstat.st_uid = node->uid;
    kstat.st_gid = node->gid;
    
    if (node->type == VFS_FILE) kstat.st_mode = S_IFREG | 0755;
    else if (node->type == VFS_DIRECTORY) kstat.st_mode = S_IFDIR | 0755;
    
    memcpy(buf, &kstat, sizeof(struct stat));
    return 0;
}

int sys_fstat(int fd, struct stat* buf) {
    if (fd < 0 || fd >= MAX_FILES) return -1;
    if (!buf || !is_user_addr(buf, sizeof(struct stat))) return -1;
    if (!current_process || !current_process->files[fd]) return -1;

    vfs_node_t* node = current_process->files[fd]->node;
    if (!node) return -1;

    struct stat kstat;
    memset(&kstat, 0, sizeof(kstat));
    kstat.st_size = node->size;
    kstat.st_uid = node->uid;
    kstat.st_gid = node->gid;
    
    if (node->type == VFS_FILE) kstat.st_mode = S_IFREG | 0755;
    else if (node->type == VFS_DIRECTORY) kstat.st_mode = S_IFDIR | 0755;

    memcpy(buf, &kstat, sizeof(struct stat));
    return 0;
}

int sys_spawn(const char* path, char** argv, char** envp) {
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
    if (!node || node->type != VFS_FILE) {
        // debugerr("[sys] Failed to find executable at %s", kpath);
        return -1;
    }

    // Copy argv and envp to kernel space
    char** k_argv = NULL;
    if (argv && is_user_addr(argv, sizeof(char*))) {
        int argc = 0;
        while (is_user_addr(&argv[argc], sizeof(char*)) && argv[argc]) argc++;
        k_argv = kmalloc(sizeof(char*) * (argc + 1));
        for (int j = 0; j < argc; j++) {
            char* u_arg = argv[j];
            char* k_arg = kmalloc(256); // Limit argument length
            int l;
            for (l = 0; l < 255; l++) {
                if (!is_user_addr(&u_arg[l], 1)) break;
                k_arg[l] = u_arg[l];
                if (k_arg[l] == '\0') break;
            }
            k_arg[l] = '\0';
            k_argv[j] = k_arg;
        }
        k_argv[argc] = NULL;
    }

    char** k_envp = NULL;
    if (envp && is_user_addr(envp, sizeof(char*))) {
        int envc = 0;
        while (is_user_addr(&envp[envc], sizeof(char*)) && envp[envc]) envc++;
        k_envp = kmalloc(sizeof(char*) * (envc + 1));
        for (int j = 0; j < envc; j++) {
            char* u_env = envp[j];
            char* k_env = kmalloc(256);
            int l;
            for (l = 0; l < 255; l++) {
                if (!is_user_addr(&u_env[l], 1)) break;
                k_env[l] = u_env[l];
                if (k_env[l] == '\0') break;
            }
            k_env[l] = '\0';
            k_envp[j] = k_env;
        }
        k_envp[envc] = NULL;
    }

    uint8_t* elf_data = NULL;
    debugln("about to reach if node->mntpoint");

    if (node->ops && node->ops->read) {
       debugln("[spawn] Node has driver ops. Reading from disk (Cluster: %d)\n", node->data);
    
       elf_data = kmalloc(node->size);
       if (!elf_data) return -1;

       int read_bytes = node->ops->read(node, elf_data, node->size, 0);
    
       if (read_bytes <= 0) {
          kfree(elf_data);
          return -1;
       }
    } else {
    // No specific driver 'read' op? Assume it's a raw pointer (CPIO/RAM)
       debugln("[spawn] No driver ops. Assuming RAM pointer: %p\n", node->data);
       elf_data = (uint8_t*)node->data;
    }

    int bytes = node->ops->read(node, elf_data, node->size, 0);
    debugln("[spawn] Read %d bytes from disk. Expected %d.\n", bytes, node->size);

    if (node->size > 0x1000) {
       debugln("[spawn] Data at 4KB offset: %02x %02x\n", elf_data[0x1000], elf_data[0x1001]);
    }

    process_t* proc = create_process_from_elf(elf_data, k_argv, k_envp);

    if (node->is_mountpoint && elf_data) {
        kfree(elf_data);
    }
    
    // Clean up temporary kernel buffers
    if (k_argv) {
        for (int j = 0; k_argv[j]; j++) kfree(k_argv[j]);
        kfree(k_argv);
    }
    if (k_envp) {
        for (int j = 0; k_envp[j]; j++) kfree(k_envp[j]);
        kfree(k_envp);
    }

    if (!proc) {
        debugerr("[sys] Failed to create process from ELF for %s", kpath);
        return -1;
    }

    if (current_process) {
        proc->parent_pid = current_process->pid;
    }
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

long sys_fork(registers_t* regs) {
    if (!current_process) return -1;
    
    extern process_t* clone_process(process_t* src, registers_t* regs);
    process_t* child = clone_process(current_process, regs);
    if (!child) return -1;
    
    add_process(child);
    return (long)child->pid;
}

long sys_execve(const char* path, char** argv, char** envp, registers_t* regs) {
    if (!current_process) return -1;
    
    char kpath[256];
    size_t i;
    for (i = 0; i < 255; i++) {
        if (!is_user_addr((void*)&path[i], 1)) return -1;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
    }
    kpath[i] = '\0';
    
    debugln("[sys] PID %d EXECVE: %s", current_process->pid, kpath);

    vfs_node_t* node = vfs_path_to_node(kpath);
    if (!node || node->type != VFS_FILE) {
        debugerr("[sys] EXECVE failed: %s not found", kpath);
        return -1;
    }

    extern int replace_process_with_elf(process_t* proc, uint8_t* elf_data, char** argv, char** envp, registers_t* regs);
    return replace_process_with_elf(current_process, (uint8_t*)node->data, argv, envp, regs);
}

long sys_ioctl(int fd, unsigned long request, void* argp) {
    if (!current_process)
        return -1;

    if (fd < 0 || fd >= MAX_FILES)
        return -1;

    vfs_file_t* file = current_process->files[fd];

    if (!file || !file->node)
        return -1;

    // tty device
    if (file->node->type == VFS_DEVICE) {
        tty_device_t* dev =
            (tty_device_t*)file->node->internal_data;

        if (dev && dev->tty) {
            return tty_ioctl(request, argp);
        }
    }

    return -1;
}

uint64_t sys_mount(uint64_t source_ptr, uint64_t target_ptr, uint64_t fstype_ptr) {
    // 1. Cast the raw register values to usable C strings
    const char* source = (const char*)source_ptr;
    const char* target = (const char*)target_ptr;
    const char* fstype = (const char*)fstype_ptr;

    // 2. Safety Check: Ensure the pointers aren't null
    if (!source || !target || !fstype) {
        return -22; // Invalid argument
    }

    // 3. Debug logging (helpful for your Znu boot logs)
    debugln("[sys] mount: dev=%s, path=%s, type=%s", source, target, fstype);

    // 4. Call your internal VFS function
    // Note: Ensure your vfs_mount argument order matches!
    // Based on your previous code, it looked like: vfs_mount(device, type, path)
    if (!vfs_mount(source, fstype, target)) {
        return -1; // Or a more specific error like -ENODEV
    }

    return 0; // Success
}

uint64_t syscall_handler(registers_t* regs) {
    uint64_t num = regs->rax;
    uint64_t arg1 = regs->rdi;
    uint64_t arg2 = regs->rsi;
    uint64_t arg3 = regs->rdx;
    uint64_t arg4 = regs->r10;
    uint64_t arg5 = regs->r8;

//    if (current_process && current_process->pid == 2) {
//        if (num == 1) {
//            // debugln("[sys] PID 2 WRITE: fd=%d, buf=%p, len=%d", (int)arg1, (void*)arg2, (int)arg3);
//        } else if (num != 0) { // Don't log read, it's noisy
//            // debugln("[sys] PID 2 SYSCALL START: %d (arg1=%d, arg2=%p)", (int)num, (int)arg1, (void*)arg2);
//        }
//    }
    switch (num) {
        case 0: // read
            return (uint64_t)sys_read((int)arg1, (void*)arg2, (size_t)arg3);
        case 1: // write
            return (uint64_t)sys_write((int)arg1, (const void*)arg2, (size_t)arg3);
        case 2: // open
            return (uint64_t)sys_open((const char*)arg1, (int)arg2);
        case 3: // close
            return (uint64_t)sys_close((int)arg1);
        case 4: // stat
            return (uint64_t)sys_stat((const char*)arg1, (struct stat*)arg2);
        case 5: // fstat
            return (uint64_t)sys_fstat((int)arg1, (struct stat*)arg2);
        case 6: // lstat
            return (uint64_t)sys_stat((const char*)arg1, (struct stat*)arg2);
        case 9: // mmap
            return (uint64_t)sys_mmap((void*)arg1, (size_t)arg2, (int)arg3, (int)arg4, (int)arg5, 0); // simplified
        case 165: // mount
            return (uint64_t)sys_mount(arg1, arg2, arg3);
        case 12: // brk
            return (uint64_t)sys_brk((void*)arg1);
        case 16: // ioctl
            return (uint64_t)sys_ioctl((int)arg1, (unsigned long)arg2, (void*)arg3);
        case 57: // fork
            return (uint64_t)sys_fork(regs);
        case 59: // execve
            return (uint64_t)sys_execve((const char*)arg1, (char**)arg2, (char**)arg3, regs);
        case 159: // spawn
            return (uint64_t)sys_spawn((const char*)arg1, (char**)arg2, (char**)arg3);
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
                extern int do_wait(int pid, int* status);
                return (uint64_t)do_wait((int)arg1, (int*)arg2);
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

