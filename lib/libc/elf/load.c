#include <elf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <page.h>
#include <proc.h>
#include <kernel/tty.h>
#include <fcntl.h>
#include <gdt.h>
#include <syscall.h>

extern void     jump_to_usermode(uintptr_t entry, uintptr_t stack);
extern uint64_t *kernel_pml4;
extern uint64_t  hhdm_offset;
extern uintptr_t vmm_virt_to_phys(uint64_t *pml4, uintptr_t virt);
extern void      vmm_free_user_pages(uint64_t *pml4);
extern void      vmm_switch(uint64_t *pml4);
extern int       get_cpu_id(void);

#define MSR_FS_BASED   0xC0000100UL
#define MSR_GS_BASE   0xC0000101UL

static inline void wrmsrl(uint32_t msr, uint64_t val)
{
    __asm__ volatile("wrmsr"
        : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32))
        : "memory");
}

static uint64_t elf_flags_to_pte(uint32_t pflags, int user)
{
    uint64_t pte = PTE_PRESENT;
    if (user)        pte |= PTE_USER;
    if (pflags & PF_W) pte |= PTE_WRITABLE;
#ifdef PTE_NX
    if (!(pflags & PF_X)) pte |= PTE_NX;
#endif
    return pte;
}

static inline void hhdm_write64(uint64_t *pml4, uintptr_t uva, uint64_t val)
{
    uintptr_t phys = vmm_virt_to_phys(pml4, uva);
    *(uint64_t *)(phys + hhdm_offset) = val;
}

static void hhdm_copy_to_user(uint64_t *pml4, uintptr_t uva,
                               const void *src, size_t len)
{
    const uint8_t *s = src;
    while (len > 0) {
        uintptr_t phys       = vmm_virt_to_phys(pml4, uva);
        uint64_t  page_off   = uva & 0xFFFULL;
        uint64_t  chunk      = 0x1000ULL - page_off;
        if (chunk > len) chunk = len;
        memcpy((void *)(phys + hhdm_offset + page_off), s, chunk);
        uva += chunk;
        s   += chunk;
        len -= chunk;
    }
}

/* Allocate and map a contiguous range of user virtual pages. */
static void map_user_range(uint64_t *pml4, uintptr_t start, uintptr_t end,
                            uint64_t pte_flags)
{
    uintptr_t page = start & ~0xFFFULL;
    uintptr_t stop = (end + 0xFFFULL) & ~0xFFFULL;
    for (; page < stop; page += 0x1000) {
        if (vmm_virt_to_phys(pml4, page) == 0) {
            void *phys = palloc_zero();
            map_page(pml4, page, (uintptr_t)phys, pte_flags);
        }
    }
}

/* Remap an already-mapped range to new flags (for RELRO). */
static void remap_user_range(uint64_t *pml4, uintptr_t start, uintptr_t end,
                              uint64_t pte_flags)
{
    uintptr_t page = start & ~0xFFFULL;
    uintptr_t stop = (end + 0xFFFULL) & ~0xFFFULL;
    for (; page < stop; page += 0x1000) {
        uintptr_t phys = vmm_virt_to_phys(pml4, page);
        if (phys) map_page(pml4, page, phys, pte_flags);
    }
}

#define TLS_USER_BASE   0x00007fff00000000ULL   /* well below stack */
#define TCB_SIZE        16                       /* self ptr + padding */

typedef struct {
    uintptr_t self;     /* %fs:0 — must equal the TCB's own address */
    uintptr_t dtv;      /* %fs:8 — unused for static TLS, zero */
} tcb_t;

static uintptr_t elf_setup_tls(process_t *proc,
                                const uint8_t *tls_image,
                                size_t tls_filesz,
                                size_t tls_memsz,
                                size_t tls_align)
{
    if (tls_align < 1) tls_align = 1;

    size_t total = tls_memsz + TCB_SIZE;
    /* Round total up to page granularity for mapping. */
    size_t total_pages = (total + 0xFFFULL) & ~0xFFFULL;

    uintptr_t uva_base = TLS_USER_BASE;
    uint64_t pte_flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
#ifdef PTE_NX
    pte_flags |= PTE_NX;
#endif

    /* Map pages for TLS block + TCB. */
    for (size_t off = 0; off < total_pages; off += 0x1000) {
        void *phys = palloc_zero();
        map_page(proc->pml4, uva_base + off, (uintptr_t)phys, pte_flags);
    }

    /* TLS data lives at uva_base; TCB immediately after. */
    uintptr_t uva_tcb  = uva_base + tls_memsz;
    uintptr_t uva_tls  = uva_base;

    /* Copy TLS initializer image (rest is already zero from palloc_zero). */
    if (tls_filesz > 0 && tls_image)
        hhdm_copy_to_user(proc->pml4, uva_tls, tls_image, tls_filesz);

    /* Write TCB.self = &TCB (required by ABI). */
    hhdm_write64(proc->pml4, uva_tcb,     uva_tcb);   /* self  */
    hhdm_write64(proc->pml4, uva_tcb + 8, 0);         /* dtv=0 */

    proc->tls_base = uva_tcb;
    proc->tls_size = (uint32_t)total;

    return uva_tcb;   /* = %fs base */
}

typedef struct {
    uint64_t type;
    uint64_t value;
} auxv_entry_t;

#define PUSH_AUXV(sp, pml4, t, v) do {        \
    (sp) -= 8; hhdm_write64(pml4, sp, (v));   \
    (sp) -= 8; hhdm_write64(pml4, sp, (t));   \
} while (0)

/* AT_* constants (supplement what elf.h may already define) */
#ifndef AT_NULL
# define AT_NULL        0
#endif
#ifndef AT_PHDR
# define AT_PHDR        3
#endif
#ifndef AT_PHENT
# define AT_PHENT       4
#endif
#ifndef AT_PHNUM
# define AT_PHNUM       5
#endif
#ifndef AT_PAGESZ
# define AT_PAGESZ      6
#endif
#ifndef AT_BASE
# define AT_BASE        7
#endif
#ifndef AT_FLAGS
# define AT_FLAGS       8
#endif
#ifndef AT_ENTRY
# define AT_ENTRY       9
#endif
#ifndef AT_UID
# define AT_UID        11
#endif
#ifndef AT_EUID
# define AT_EUID       12
#endif
#ifndef AT_GID
# define AT_GID        13
#endif
#ifndef AT_EGID
# define AT_EGID       14
#endif
#ifndef AT_CLKTCK
# define AT_CLKTCK     17
#endif
#ifndef AT_SECURE
# define AT_SECURE     23
#endif
#ifndef AT_RANDOM
# define AT_RANDOM     25
#endif
#ifndef AT_HWCAP
# define AT_HWCAP      16
#endif
#ifndef AT_EXECFN
# define AT_EXECFN     31
#endif
#ifndef AT_PLATFORM
# define AT_PLATFORM   15
#endif
#ifndef AT_SYSINFO_EHDR
# define AT_SYSINFO_EHDR 33
#endif

static uintptr_t push_random_bytes(uint64_t *pml4, uintptr_t *sp)
{
    static uint64_t seed = 0xDEADBEEFCAFEBABEULL;
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    uint64_t r0 = seed;
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    uint64_t r1 = seed;

    *sp -= 8; hhdm_write64(pml4, *sp, r1);
    *sp -= 8; hhdm_write64(pml4, *sp, r0);
    return *sp;
}

static uintptr_t elf_build_stack(process_t *proc,
                                  uintptr_t stack_top,
                                  char **argv, int argc,
                                  char **envp, int envc,
                                  const Elf64_Ehdr *hdr,
                                  uintptr_t load_base,
                                  uintptr_t phdr_addr,
                                  uintptr_t tls_base,
                                  const char *execfn)
{
    uint64_t *pml4  = proc->pml4;
    uintptr_t sp    = stack_top;


    uintptr_t *uenvp = kmalloc(sizeof(uintptr_t) * (envc + 1));
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        sp -= len;
        sp &= ~7ULL;
        hhdm_copy_to_user(pml4, sp, envp[i], len);
        uenvp[i] = sp;
    }
    uenvp[envc] = 0;

    uintptr_t *uargv = kmalloc(sizeof(uintptr_t) * (argc + 1));
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        sp &= ~7ULL;
        hhdm_copy_to_user(pml4, sp, argv[i], len);
        uargv[i] = sp;
    }
    uargv[argc] = 0;

    /* execfn string */
    uintptr_t u_execfn = 0;
    if (execfn) {
        size_t len = strlen(execfn) + 1;
        sp -= len;
        sp &= ~7ULL;
        hhdm_copy_to_user(pml4, sp, execfn, len);
        u_execfn = sp;
    }

    /* "x86_64" platform string */
    const char *platform_str = "x86_64";
    sp -= strlen(platform_str) + 1;
    sp &= ~7ULL;
    hhdm_copy_to_user(pml4, sp, platform_str, strlen(platform_str) + 1);
    uintptr_t u_platform = sp;

    /* AT_RANDOM 16-byte blob */
    uintptr_t u_random = push_random_bytes(pml4, &sp);

    /* Count slots to be pushed: auxv + envp_ptrs + argv_ptrs + argc */
    int n_auxv = 14; /* AT_NULL + 6 pairs → 14 slots (7 entries × 2) */
    int total_slots = n_auxv + (envc + 1) + (argc + 1) + 1;
    if (total_slots & 1) sp -= 8;  /* alignment pad */
    sp &= ~0xFULL;                 /* final 16-byte align */

    PUSH_AUXV(sp, pml4, AT_NULL,    0);
    PUSH_AUXV(sp, pml4, AT_PLATFORM, u_platform);
    if (u_execfn) PUSH_AUXV(sp, pml4, AT_EXECFN, u_execfn);
    PUSH_AUXV(sp, pml4, AT_RANDOM,  u_random);
    PUSH_AUXV(sp, pml4, AT_SECURE,  0);
    PUSH_AUXV(sp, pml4, AT_EGID,    0);
    PUSH_AUXV(sp, pml4, AT_GID,     0);
    PUSH_AUXV(sp, pml4, AT_EUID,    0);
    PUSH_AUXV(sp, pml4, AT_UID,     0);
    PUSH_AUXV(sp, pml4, AT_CLKTCK,  100);
    PUSH_AUXV(sp, pml4, AT_FLAGS,   0);
    PUSH_AUXV(sp, pml4, AT_BASE,    0);    /* 0 = no interpreter */
    PUSH_AUXV(sp, pml4, AT_HWCAP,   0);   /* fill from CPUID if desired */
    PUSH_AUXV(sp, pml4, AT_PAGESZ,  0x1000);
    PUSH_AUXV(sp, pml4, AT_ENTRY,   hdr->e_entry);
    PUSH_AUXV(sp, pml4, AT_PHENT,   hdr->e_phentsize);
    PUSH_AUXV(sp, pml4, AT_PHNUM,   hdr->e_phnum);
    PUSH_AUXV(sp, pml4, AT_PHDR,    load_base + hdr->e_phoff);

    sp -= 8; hhdm_write64(pml4, sp, 0);           /* NULL terminator */
    for (int i = envc - 1; i >= 0; i--) {
        sp -= 8; hhdm_write64(pml4, sp, uenvp[i]);
    }

    sp -= 8; hhdm_write64(pml4, sp, 0);           /* NULL terminator */
    for (int i = argc - 1; i >= 0; i--) {
        sp -= 8; hhdm_write64(pml4, sp, uargv[i]);
    }

    sp -= 8; hhdm_write64(pml4, sp, (uint64_t)argc);

    kfree(uenvp);
    kfree(uargv);

    return sp;
}

uint64_t *vmm_create_user_pml4(void)
{
    uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(palloc_zero());
    /* Copy higher-half kernel entries */
    for (int i = 256; i < 512; i++)
        pml4[i] = kernel_pml4[i];
    return pml4;
}

static int elf_validate(const Elf64_Ehdr *hdr)
{
    if (memcmp(hdr->e_ident, ELFMAG, 4) != 0)         { debugln("[elf] Bad magic");           return -1; }
    if (hdr->e_ident[EI_CLASS]   != ELFCLASS64)        { debugln("[elf] Not ELF64");           return -1; }
    if (hdr->e_ident[EI_DATA]    != ELFDATA2LSB)        { debugln("[elf] Not little-endian");   return -1; }
    if (hdr->e_machine           != EM_X86_64)          { debugln("[elf] Not x86-64");          return -1; }
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN && hdr->e_type != ET_REL){ debugln("[elf] Not executable/dynamic/relocatable"); return -1; }
    return 0;
}

static int elf_load_segments(process_t *proc,
                              const uint8_t *elf_data,
                              const Elf64_Ehdr *hdr,
                              uintptr_t *out_load_base,
                              uintptr_t *out_max_vaddr,
                              uintptr_t *out_tls_base,
                              int *out_stack_exec)
{
    const Elf64_Phdr *phdr  = (const Elf64_Phdr *)(elf_data + hdr->e_phoff);
    uintptr_t load_base     = UINTPTR_MAX;
    uintptr_t max_vaddr     = 0;
    uintptr_t tls_base      = 0;
    int       stack_exec    = 0;

    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_INTERP) {
            debugln("[elf] PT_INTERP found — dynamic executables not yet supported");
            return -1;
        }
    }

    for (int i = 0; i < hdr->e_phnum; i++) {
        const Elf64_Phdr *p = &phdr[i];

        switch (p->p_type) {

        case PT_LOAD: {
            if (p->p_memsz == 0) continue;

            uint64_t pte_flags = elf_flags_to_pte(p->p_flags, 1 /*user*/);

            uintptr_t seg_start = p->p_vaddr & ~0xFFFULL;
            uintptr_t seg_end   = (p->p_vaddr + p->p_memsz + 0xFFFULL) & ~0xFFFULL;

            map_user_range(proc->pml4, seg_start, seg_end, pte_flags);

            /* Copy initialised data via HHDM */
            if (p->p_filesz > 0)
                hhdm_copy_to_user(proc->pml4, p->p_vaddr,
                                  elf_data + p->p_offset, p->p_filesz);

            /* Track address range */
            if (p->p_vaddr < load_base) load_base = p->p_vaddr;
            uintptr_t seg_top = p->p_vaddr + p->p_memsz;
            if (seg_top > max_vaddr) max_vaddr = seg_top;
            break;
        }

        case PT_TLS: {
            const uint8_t *tls_image = (p->p_filesz > 0)
                                       ? elf_data + p->p_offset
                                       : NULL;
            tls_base = elf_setup_tls(proc,
                                     tls_image,
                                     p->p_filesz,
                                     p->p_memsz,
                                     p->p_align);
            if (!tls_base) {
                debugln("[elf] TLS setup failed");
                return -1;
            }
            break;
        }

        case PT_GNU_STACK:
            /* PF_X set → executable stack requested */
            stack_exec = !!(p->p_flags & PF_X);
            break;

        /* PT_GNU_RELRO handled in a third pass after all loads */
        case PT_GNU_RELRO:
        case PT_PHDR:
        case PT_NOTE:
        case PT_NULL:
        default:
            break;
        }
    }

    for (int i = 0; i < hdr->e_phnum; i++) {
        const Elf64_Phdr *p = &phdr[i];
        if (p->p_type == PT_GNU_RELRO) {
            /* Remove PTE_WRITABLE from the RELRO range */
            uint64_t ro_flags = PTE_PRESENT | PTE_USER;
#ifdef PTE_NX
            ro_flags |= PTE_NX;
#endif
            remap_user_range(proc->pml4,
                             p->p_vaddr,
                             p->p_vaddr + p->p_memsz,
                             ro_flags);
        }
    }

    if (load_base == UINTPTR_MAX) load_base = 0;

    *out_load_base  = load_base;
    *out_max_vaddr  = max_vaddr;
    *out_tls_base   = tls_base;
    *out_stack_exec = stack_exec;
    return 0;
}

#define USER_STACK_BASE  0x00007ffff0000000ULL
#define STACK_PAGES      16          /* 64 KiB */

static uintptr_t alloc_user_stack(uint64_t *pml4, int stack_exec)
{
    uint64_t pte = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
#ifdef PTE_NX
    if (!stack_exec) pte |= PTE_NX;
#endif
    for (int i = 0; i < STACK_PAGES; i++) {
        void *phys = palloc_zero();
        map_page(pml4, USER_STACK_BASE + (uintptr_t)i * 0x1000,
                 (uintptr_t)phys, pte);
    }
    return USER_STACK_BASE + (uintptr_t)STACK_PAGES * 0x1000;
}

static void init_regs(process_t *proc, uintptr_t entry, uintptr_t stack_top)
{
    registers_t *regs = (registers_t *)(proc->kstack_top - sizeof(registers_t));
    memset(regs, 0, sizeof(registers_t));

    regs->rip    = entry;
    regs->rsp    = stack_top;
    regs->cs     = 0x23;   /* user code  (ring 3) */
    regs->ss     = 0x1B;   /* user data  (ring 3) */
    regs->ds     = 0x1B;
    regs->es     = 0x1B;
    regs->rflags = 0x202;  /* IF=1 */
    regs->rax    = 0;

    proc->context_ptr = regs;
}

static void init_fpu(process_t *proc)
{
    static uint8_t fxbuf[512] __attribute__((aligned(16)));
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : "=m"(*fxbuf));
    /* Set MXCSR default (0x1F80) */
    *(uint32_t *)(fxbuf + 24) = 0x1F80;
    memcpy(proc->sse_state, fxbuf, 512);
}

uint64_t next_pid = 1;

process_t *create_process_from_elf(uint8_t *elf_data, char **argv, char **envp)
{
    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;
    if (elf_validate(hdr) < 0) return NULL;

    process_t *proc = kmalloc(sizeof(process_t));
    memset(proc, 0, sizeof(process_t));

    proc->pid        = next_pid++;
    proc->pml4       = vmm_create_user_pml4();
    proc->state      = TASK_READY;
    proc->parent_pid = current_process ? current_process->pid : 0;

    /* Kernel stack */
    void *kstack       = kmalloc(32768);
    proc->kstack_top   = ((uintptr_t)kstack + 32768) & ~0xFULL;

    if (proc->pid == 1) init_process = proc;

    /* Load segments */
    uintptr_t load_base, max_vaddr, tls_base;
    int stack_exec;
    if (elf_load_segments(proc, elf_data, hdr,
                          &load_base, &max_vaddr, &tls_base, &stack_exec) < 0) {
        kfree(kstack);
        kfree(proc);
        return NULL;
    }

    proc->brk_start = (max_vaddr + 0xFFFULL) & ~0xFFFULL;
    proc->brk       = proc->brk_start;
    proc->entry     = hdr->e_entry;

    /* User stack */
    uintptr_t stack_top = alloc_user_stack(proc->pml4, stack_exec);

    /* Count argv/envp */
    int argc = 0; if (argv) while (argv[argc]) argc++;
    int envc = 0; if (envp) while (envp[envc]) envc++;

    /* Build stack (strings + auxv + pointers + argc) */
    const char *execfn = (argc > 0) ? argv[0] : NULL;
    uintptr_t rsp = elf_build_stack(proc, stack_top,
                                    argv, argc, envp, envc,
                                    hdr, load_base, tls_base,
                                    tls_base, execfn);
    proc->stack_top = rsp;

    /* Kernel-side register frame */
    init_regs(proc, proc->entry, proc->stack_top);
    init_fpu(proc);

    /* Stdio */
    for (int i = 0; i < MAX_FILES; i++) proc->files[i] = NULL;
    proc->files[0] = tty_open_file(0, O_RDONLY);
    proc->files[1] = tty_open_file(0, O_WRONLY);
    proc->files[2] = tty_open_file(0, O_WRONLY);

    /* TLS %fs base is written to MSR when the process first runs.
     * Store it so the context switcher can do: wrmsrl(MSR_FS_BASED, proc->tls_base) */

    return proc;
}

int replace_process_with_elf(process_t *proc, uint8_t *elf_data,
                              char **argv, char **envp, registers_t *regs)
{

    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;
    if (elf_validate(hdr) < 0)
        return -1;

    /* Count user-pointer arrays before touching any page tables. */
    int argc = 0; if (argv) while (argv[argc]) argc++;
    int envc = 0; if (envp) while (envp[envc]) envc++;

    /*
     * Deep-copy argv[] and envp[] strings onto the kernel heap NOW.
     * After vmm_switch() the old user mappings are gone; any deferred
     * access to the original user pointers would triple-fault.
     */
    char **kargv = kmalloc(sizeof(char *) * (argc + 1));
    if (!kargv) return -1;
    memset(kargv, 0, sizeof(char *) * (argc + 1));

    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        kargv[i] = kmalloc(len);
        if (!kargv[i]) goto fail_before_pml4;
        memcpy(kargv[i], argv[i], len);
    }

    char **kenvp = kmalloc(sizeof(char *) * (envc + 1));
    if (!kenvp) goto fail_before_pml4;
    memset(kenvp, 0, sizeof(char *) * (envc + 1));

    for (int i = 0; i < envc; i++) {
        size_t len = strlen(envp[i]) + 1;
        kenvp[i] = kmalloc(len);
        if (!kenvp[i]) goto fail_before_pml4;
        memcpy(kenvp[i], envp[i], len);
    }

    uint64_t *new_pml4 = vmm_create_user_pml4();
    if (!new_pml4) goto fail_before_pml4;

    /*
     * Use a lightweight scratch process shell so that elf_load_segments()
     * and elf_build_stack() write into new_pml4 rather than proc->pml4.
     * Only pml4, tls_base, and tls_size are used by those helpers.
     */
    process_t staging;
    memset(&staging, 0, sizeof(staging));
    staging.pml4 = new_pml4;

    uintptr_t load_base = 0, max_vaddr = 0, tls_base = 0;
    int stack_exec = 0;

    if (elf_load_segments(&staging, elf_data, hdr,
                          &load_base, &max_vaddr,
                          &tls_base, &stack_exec) < 0) {
        debugln("[elf] execve: segment loading failed — aborting, old AS intact");
        goto fail_after_pml4;
    }

    uintptr_t new_stack_top = alloc_user_stack(new_pml4, stack_exec);
    if (!new_stack_top) {
        debugln("[elf] execve: stack allocation failed — aborting, old AS intact");
        goto fail_after_pml4;
    }

    /*
     * elf_build_stack writes strings and pointer arrays into new_pml4 via
     * HHDM.  It returns the final RSP value (points to argc on the stack).
     * A return value of 0 indicates a vmm_virt_to_phys miss on a stack page
     * — treat it as a fatal staging error.
     */
    staging.tls_base = tls_base;
    const char *execfn = (argc > 0) ? kargv[0] : NULL;
    uintptr_t new_rsp  = elf_build_stack(&staging, new_stack_top,
                                         kargv, argc, kenvp, envc,
                                         hdr, load_base, tls_base,
                                         tls_base, execfn);
    if (!new_rsp) {
        debugln("[elf] execve: stack build failed — aborting, old AS intact");
        goto fail_after_pml4;
    }

    uint64_t *old_pml4 = proc->pml4;   /* (a) cache before overwriting */

    /* (b) Update proc metadata — no page table access yet */
    proc->pml4      = new_pml4;
    proc->entry     = hdr->e_entry;
    proc->stack_top = new_rsp;
    proc->brk_start = (max_vaddr + 0xFFFULL) & ~0xFFFULL;
    proc->brk       = proc->brk_start;
    proc->tls_base  = tls_base;

    /* Close non-stdio file descriptors (exec semantics).
     * Done before vmm_switch because vfs_close may need the old AS. */
    for (int i = 3; i < MAX_FILES; i++) {
        if (proc->files[i]) {
            /* vfs_close(proc->files[i]); */
            proc->files[i] = NULL;
        }
    }

    /* (c) Commit: switch CR3 — from this point the new address space is live */
    vmm_switch(proc->pml4);

    /* (d) Free old address space — safe now that CR3 no longer references it */
    vmm_free_user_pages(old_pml4);
    /* old_pml4 itself is a kernel-heap or palloc'd page; free the PML4 frame */
    pfree((void *)VIRT_TO_PHYS((uintptr_t)old_pml4));

    int cpu = get_cpu_id();

    /*
     * TSS.RSP0: used by the CPU on privilege transitions triggered by
     * hardware interrupts while the process is in user mode.  Must point
     * to the top of the current process's kernel stack.
     */
    tss_per_cpu[cpu].rsp0 = proc->kstack_top;

    extern cpu_context_t cpu_contexts[MAX_CPUS];
    cpu_contexts[cpu].kernel_stack = proc->kstack_top;

    memset(regs, 0, sizeof(registers_t));
    regs->rip    = proc->entry;
    regs->rsp    = proc->stack_top;
    regs->cs     = 0x23;    /* ring-3 code segment */
    regs->ss     = 0x1B;    /* ring-3 data segment */
    regs->ds     = 0x1B;
    regs->es     = 0x1B;
    regs->rflags = 0x202;   /* IF=1, everything else clean */
    /* All GPRs already zeroed by memset above */

    proc->context_ptr = regs;

    /* Write %fs base for TLS.  The context switcher must also do this on
     * every task-switch-in: wrmsrl(MSR_FS_BASED, current_process->tls_base) */
    if (tls_base)
        wrmsrl(MSR_FS_BASED, tls_base);
    else
        wrmsrl(MSR_FS_BASED, 0);

    /* Reset FPU/SSE state to a clean MXCSR=0x1F80 baseline */
    init_fpu(proc);

    /* Free kernel-heap staging copies */
    for (int i = 0; i < argc; i++) kfree(kargv[i]);
    kfree(kargv);
    for (int i = 0; i < envc; i++) kfree(kenvp[i]);
    kfree(kenvp);

    debugln("[elf] execve: committed pid=%llu entry=%lx rsp=%lx tls=%lx",
            (unsigned long long)proc->pid,
            (unsigned long)proc->entry,
            (unsigned long)proc->stack_top,
            (unsigned long)tls_base);

    return 0;

fail_after_pml4:
    /* Staged new PML4 is partially built — tear it down cleanly */
    vmm_free_user_pages(new_pml4);
    pfree((void *)VIRT_TO_PHYS((uintptr_t)new_pml4));
    /* fall through */

fail_before_pml4:
    /* Free any argv/envp strings that were successfully copied */
    if (kargv) {
        for (int i = 0; i < argc; i++) if (kargv[i]) kfree(kargv[i]);
        kfree(kargv);
    }
    if (kenvp) {
        for (int i = 0; i < envc; i++) if (kenvp[i]) kfree(kenvp[i]);
        kfree(kenvp);
    }
    return -1;
}

/*
 * load_elf — boot-time helper to load and jump to the init process
 * directly (no process_t, runs on kernel PML4).  Kept for compatibility.
 */
void load_elf(uint8_t *elf_data)
{
    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;
    if (elf_validate(hdr) < 0) {
        debugln("[elf] load_elf: invalid ELF");
        return;
    }

    const Elf64_Phdr *phdr = (const Elf64_Phdr *)(elf_data + hdr->e_phoff);

    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        if (phdr[i].p_memsz == 0) continue;

        uintptr_t seg_start = phdr[i].p_vaddr & ~0xFFFULL;
        uintptr_t seg_end   = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFFULL) & ~0xFFFULL;
        uint64_t  pte       = elf_flags_to_pte(phdr[i].p_flags, 1);

        for (uintptr_t page = seg_start; page < seg_end; page += 0x1000) {
            if (vmm_virt_to_phys(kernel_pml4, page) == 0) {
                void *phys = palloc_zero();
                map_page(kernel_pml4, page, (uintptr_t)phys, pte);
            }
        }

        if (phdr[i].p_filesz > 0)
            hhdm_copy_to_user(kernel_pml4, phdr[i].p_vaddr,
                              elf_data + phdr[i].p_offset, phdr[i].p_filesz);
    }

    uintptr_t stack_top = alloc_user_stack(kernel_pml4, 0 /*NX stack*/);
    uintptr_t sp = stack_top - 16;
    /* No auxv for the raw boot path — _start will get argc=0 */
    *(uint64_t *)(vmm_virt_to_phys(kernel_pml4, sp) + hhdm_offset) = 0; /* argc */

    debugln("[elf] Jumping to entry %lx, rsp %lx", hdr->e_entry, sp);
    jump_to_usermode(hdr->e_entry, sp);
}
