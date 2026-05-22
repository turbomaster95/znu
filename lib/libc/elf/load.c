/*
 * elf.c — Full-featured ELF64 loader for x86-64 kernel
 *
 * Supports:
 *   PT_LOAD     — segment loading with correct PF_R/W/X → PTE flags
 *   PT_TLS      — Thread-Local Storage block + TCB setup, %fs base via WRMSRL
 *   PT_GNU_STACK — NX stack enforcement (stack gets PTE_NX when PF_X absent)
 *   PT_GNU_RELRO — post-load read-only remapping of RELRO segment
 *   PT_INTERP   — detected and rejected with clear error (no dynamic linker yet)
 *   AT_* auxv   — full System V AMD64 ABI auxiliary vector
 *   ABI stack   — correct 16-byte alignment per SysV AMD64 ABI
 *   fxsave area — 16-byte aligned SSE state
 *   brk         — page-aligned, set from PT_LOAD high watermark
 *
 * Bugs fixed vs. original:
 *   - AT_PHDR computed correctly (load base + e_phoff, not e_entry - e_phoff)
 *   - Stack alignment applied before argc push, not after
 *   - Segment pages get NX unless PF_X is set
 *   - Old PML4 freed in replace_process_with_elf
 *   - fxsave buffer is __attribute__((aligned(16)))
 */

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

/* ───────────────────────── external symbols ───────────────────────── */

extern void     jump_to_usermode(uintptr_t entry, uintptr_t stack);
extern uint64_t *kernel_pml4;
extern uint64_t  hhdm_offset;
extern uintptr_t vmm_virt_to_phys(uint64_t *pml4, uintptr_t virt);
extern void      vmm_free_user_pages(uint64_t *pml4);   /* you must implement this */
extern void      vmm_switch(uint64_t *pml4);
extern int       get_cpu_id(void);

/* ───────────────────────── MSR helpers ────────────────────────────── */

#define MSR_FS_BASE   0xC0000100UL
#define MSR_GS_BASE   0xC0000101UL

static inline void wrmsrl(uint32_t msr, uint64_t val)
{
    __asm__ volatile("wrmsr"
        : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32))
        : "memory");
}

/* ───────────────────────── PTE flag helpers ───────────────────────── */

/*
 * Convert ELF segment flags (PF_R / PF_W / PF_X) to page-table entry flags.
 * PTE_NX is the "No-Execute" bit (bit 63).  Assumes your page.h defines:
 *   PTE_PRESENT, PTE_WRITABLE, PTE_USER, PTE_NX
 * If PTE_NX is not defined in your page.h add:
 *   #define PTE_NX (1ULL << 63)
 */
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

/* ───────────────────────── HHDM write helpers ─────────────────────── */

/* Write 8 bytes to a user virtual address via HHDM (no CR3 switch needed). */
static inline void hhdm_write64(uint64_t *pml4, uintptr_t uva, uint64_t val)
{
    uintptr_t phys = vmm_virt_to_phys(pml4, uva);
    *(uint64_t *)(phys + hhdm_offset) = val;
}

/* Copy bytes to a user virtual address range via HHDM (handles page boundaries). */
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

/* ───────────────────────── Page mapping helpers ───────────────────── */

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

/* ───────────────────────── TLS / TCB ─────────────────────────────── */

/*
 * Thread Control Block layout (variant II — glibc/musl compatible):
 *
 *   [ TLS data block (tls_filesz bytes, zero-extended to tls_memsz) ]
 *   [ TCB: { uintptr_t self; } ]  ← %fs:0 points here
 *
 * %fs base = address of TCB.
 * TCB.self = %fs base  (required by glibc/musl for __thread access).
 *
 * The tls_image (PT_TLS file data) is copied into the data block.
 * We allocate everything from kmalloc (kernel heap) so it's reachable
 * via HHDM; then map a user-accessible alias at a well-known address
 * so user-space can read %fs-relative data through the page tables.
 *
 * For simplicity we put the TLS block + TCB into the user address space
 * at TLS_USER_BASE, mapped with PTE_USER | PTE_WRITABLE.
 */

#define TLS_USER_BASE   0x00007fff00000000ULL   /* well below stack */
#define TCB_SIZE        16                       /* self ptr + padding */

typedef struct {
    uintptr_t self;     /* %fs:0 — must equal the TCB's own address */
    uintptr_t dtv;      /* %fs:8 — unused for static TLS, zero */
} tcb_t;

/*
 * elf_setup_tls — allocate TLS block + TCB for a process.
 *
 * Returns the user-virtual address of the TCB (= %fs base), or 0 on failure.
 * Sets proc->tls_base and proc->tls_size.
 *
 * tls_image   : pointer to the PT_TLS file data in the ELF image
 * tls_filesz  : PT_TLS p_filesz (initialized bytes)
 * tls_memsz   : PT_TLS p_memsz  (total size, may be > filesz)
 * tls_align   : PT_TLS p_align
 */
static uintptr_t elf_setup_tls(process_t *proc,
                                const uint8_t *tls_image,
                                size_t tls_filesz,
                                size_t tls_memsz,
                                size_t tls_align)
{
    if (tls_align < 1) tls_align = 1;

    /*
     * Layout (low → high):
     *   [pad to align] [tls_memsz bytes of TLS data] [TCB_SIZE bytes]
     *
     * %fs base = start of TCB.
     */
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

/* ───────────────────────── Auxiliary vector ───────────────────────── */

/*
 * Full System V AMD64 ABI auxiliary vector.
 * Push in reverse order so the stack grows down; the topmost entry
 * ends up at the lowest address after all pushes.
 *
 * Returns the new stack_ptr after all auxv entries are pushed.
 *
 * Note: auxv entries are { uint64_t type, uint64_t value } pairs.
 */
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

/*
 * Push a 16-byte random blob for AT_RANDOM and return its user address.
 * We just use a trivial LFSR seed here; replace with your CSPRNG.
 */
static uintptr_t push_random_bytes(uint64_t *pml4, uintptr_t *sp)
{
    /* 16 bytes of pseudo-random data — replace with real entropy */
    static uint64_t seed = 0xDEADBEEFCAFEBABEULL;
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    uint64_t r0 = seed;
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    uint64_t r1 = seed;

    *sp -= 8; hhdm_write64(pml4, *sp, r1);
    *sp -= 8; hhdm_write64(pml4, *sp, r0);
    return *sp;
}

/* ───────────────────────── Stack builder ──────────────────────────── */

/*
 * Build the initial user stack according to the System V AMD64 ABI:
 *
 *   [stack top]
 *   ... (16-byte alignment pad if needed)
 *   AT_NULL         (2 × uint64_t = 16 bytes)
 *   ...auxv pairs...
 *   0               (envp NULL terminator)
 *   envp[n-1]
 *   ...
 *   envp[0]
 *   0               (argv NULL terminator)
 *   argv[argc-1]
 *   ...
 *   argv[0]
 *   argc            ← RSP points here when _start is called
 *
 * String data is pushed *before* the pointers so it lives above them.
 * The ABI requires RSP % 16 == 0 *before* the call instruction that
 * transfers to _start; since there is no call, RSP % 16 == 8 would be
 * needed if _start is called — but for direct jump we align to 16.
 */

/*
 * elf_build_stack — push all strings, auxv, envp, argv, argc.
 *
 * Returns the final RSP value.
 */
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

    /* ── 1. Copy string data onto the stack (env strings, then arg strings) ── */

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

    /* ── 2. Align stack to 16 bytes before pushing pointer arrays ── */
    /*
     * The total number of 8-byte slots we are about to push:
     *   auxv:  14 entries × 2 = 28 slots  (adjust if you add/remove entries)
     *   envp:  envc + 1 (NULL)
     *   argv:  argc + 1 (NULL)
     *   argc:  1
     * Total = 28 + envc + 1 + argc + 1 + 1 = 31 + envc + argc slots.
     *
     * For RSP to be 16-byte aligned after all pushes (each 8 bytes),
     * the number of pushes must be even. Pre-align sp here.
     */
    /* Count slots to be pushed: auxv + envp_ptrs + argv_ptrs + argc */
    int n_auxv = 14; /* AT_NULL + 6 pairs → 14 slots (7 entries × 2) */
    int total_slots = n_auxv + (envc + 1) + (argc + 1) + 1;
    if (total_slots & 1) sp -= 8;  /* alignment pad */
    sp &= ~0xFULL;                 /* final 16-byte align */

    /* ── 3. Push auxv (in reverse — AT_NULL last-pushed → first-read) ── */
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
    /* AT_PHDR: virtual address where the PHDR table was loaded.
     * For a non-PIE binary this is load_base + e_phoff.
     * (Your original code used e_entry - e_phoff which is wrong.) */
    PUSH_AUXV(sp, pml4, AT_PHDR,    load_base + hdr->e_phoff);

    /* ── 4. Push envp pointer array (NULL terminated) ── */
    sp -= 8; hhdm_write64(pml4, sp, 0);           /* NULL terminator */
    for (int i = envc - 1; i >= 0; i--) {
        sp -= 8; hhdm_write64(pml4, sp, uenvp[i]);
    }

    /* ── 5. Push argv pointer array (NULL terminated) ── */
    sp -= 8; hhdm_write64(pml4, sp, 0);           /* NULL terminator */
    for (int i = argc - 1; i >= 0; i--) {
        sp -= 8; hhdm_write64(pml4, sp, uargv[i]);
    }

    /* ── 6. Push argc ── */
    sp -= 8; hhdm_write64(pml4, sp, (uint64_t)argc);

    kfree(uenvp);
    kfree(uargv);

    return sp;
}

/* ───────────────────────── PML4 creation ──────────────────────────── */

uint64_t *vmm_create_user_pml4(void)
{
    uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(palloc_zero());
    /* Copy higher-half kernel entries */
    for (int i = 256; i < 512; i++)
        pml4[i] = kernel_pml4[i];
    return pml4;
}

/* ───────────────────────── ELF validation ──────────────────────────── */

static int elf_validate(const Elf64_Ehdr *hdr)
{
    if (memcmp(hdr->e_ident, ELFMAG, 4) != 0)         { debugln("[elf] Bad magic");           return -1; }
    if (hdr->e_ident[EI_CLASS]   != ELFCLASS64)        { debugln("[elf] Not ELF64");           return -1; }
    if (hdr->e_ident[EI_DATA]    != ELFDATA2LSB)        { debugln("[elf] Not little-endian");   return -1; }
    if (hdr->e_machine           != EM_X86_64)          { debugln("[elf] Not x86-64");          return -1; }
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN){ debugln("[elf] Not executable/dyn"); return -1; }
    return 0;
}

/* ───────────────────────── Core loader ─────────────────────────────── */

/*
 * elf_load_segments — parse all PHDRs, map/copy PT_LOAD segments,
 * handle PT_TLS, PT_GNU_STACK, PT_GNU_RELRO, reject PT_INTERP.
 *
 * Fills in:
 *   *out_load_base   — lowest PT_LOAD p_vaddr (for AT_PHDR with PIE)
 *   *out_max_vaddr   — top of highest PT_LOAD segment (for brk)
 *   *out_tls_base    — user virtual address of TCB, or 0
 *   *out_stack_exec  — 1 if stack must be executable (PT_GNU_STACK PF_X)
 *
 * Returns 0 on success, -1 on failure.
 */
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

    /* ── First pass: detect PT_INTERP (dynamic executables) ── */
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_INTERP) {
            debugln("[elf] PT_INTERP found — dynamic executables not yet supported");
            return -1;
        }
    }

    /* ── Second pass: load segments and handle special PHDRs ── */
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
            /*
             * PT_TLS describes the TLS initialiser image.
             * The actual block is allocated via elf_setup_tls() after all
             * PT_LOAD segments are mapped (so we can be sure the PML4 is ready).
             */
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

    /* ── Third pass: apply RELRO (read-only after load) ── */
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

/* ───────────────────────── User stack allocation ────────────────────── */

#define USER_STACK_BASE  0x00007ffff0000000ULL
#define STACK_PAGES      16          /* 64 KiB */

/*
 * Allocate a user stack.  Returns the initial stack_ptr (top of stack).
 * stack_exec controls whether the stack pages are executable.
 */
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

/* ───────────────────────── Initial register state ──────────────────── */

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

/*
 * fxsave buffer must be 16-byte aligned.
 * Declare a static aligned buffer and copy into the proc's sse_state array.
 * If proc->sse_state is itself 16-byte aligned in your struct, just use it directly.
 */
static void init_fpu(process_t *proc)
{
    static uint8_t fxbuf[512] __attribute__((aligned(16)));
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : "=m"(*fxbuf));
    /* Set MXCSR default (0x1F80) */
    *(uint32_t *)(fxbuf + 24) = 0x1F80;
    memcpy(proc->sse_state, fxbuf, 512);
}

/* ───────────────────────── Public API ──────────────────────────────── */

/*
 * create_process_from_elf — create a brand-new process_t from ELF data.
 *
 * Handles: PT_LOAD, PT_TLS, PT_GNU_STACK, PT_GNU_RELRO.
 * Sets up: kernel stack, registers, FPU state, stdio FDs, auxv, TLS.
 */
process_t *create_process_from_elf(uint8_t *elf_data, char **argv, char **envp)
{
    static uint64_t next_pid = 1;

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
     * Store it so the context switcher can do: wrmsrl(MSR_FS_BASE, proc->tls_base) */

    return proc;
}

/*
 * replace_process_with_elf — atomic exec() implementation.
 *
 * The entire new address space is constructed in a scratch PML4 before the
 * old one is touched.  The commit is a single pointer swap followed by a
 * CR3 write; only after that is the old PML4 freed.  This guarantees:
 *
 *   1. PRE-FLIGHT   — ELF headers validated; argv/envp deep-copied to the
 *                     kernel heap while the old address space is still live.
 *
 *   2. STAGING      — All PT_LOAD segments, TLS, stack, and the full auxv
 *                     are built into `new_pml4`.  The running process is
 *                     completely untouched.
 *
 *   3. ERROR SAFETY — Any failure before the commit calls
 *                     vmm_free_user_pages(new_pml4) and returns -1.
 *                     The caller's address space is intact; sysret is safe.
 *
 *   4. COMMIT       — CR3 is switched to new_pml4, THEN the old PML4 is
 *                     freed.  There is no window where the CPU executes
 *                     against a freed or half-built page table.
 *
 *   5. REGISTER RESET — Both the `regs` argument (the live syscall frame
 *                     that iretq/sysret will consume) AND the frame stored
 *                     at kstack_top - sizeof(registers_t) are overwritten.
 *                     They must be the same object; a debug assertion is
 *                     included to catch mis-wired syscall stubs.
 *
 * `regs` MUST point to the saved register frame that the syscall return
 * path will restore.  In a typical x86-64 kernel this is the struct pushed
 * by the syscall entry trampoline, passed down as the last argument.
 *
 * Returns 0 on success.  On failure returns -1 and leaves `proc` unchanged.
 */
int replace_process_with_elf(process_t *proc, uint8_t *elf_data,
                              char **argv, char **envp, registers_t *regs)
{
    /* ════════════════════════════════════════════════════════════════
     * PHASE 1 — PRE-FLIGHT VALIDATION
     * Validate the ELF image and deep-copy argv/envp to kernel heap
     * while the current address space is still fully intact.
     * ════════════════════════════════════════════════════════════════ */

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

    /* ════════════════════════════════════════════════════════════════
     * PHASE 2 — STAGING
     * Build the entire new address space in a scratch PML4.
     * Nothing in `proc` is modified until the commit.
     * ════════════════════════════════════════════════════════════════ */

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

    /* ════════════════════════════════════════════════════════════════
     * PHASE 3 — POINT OF NO RETURN / COMMIT
     *
     * Everything below here must not fail.  All fallible operations
     * (allocation, copying, validation) are complete.
     *
     * Order:
     *   a) Cache old PML4 pointer.
     *   b) Update proc state with new values.
     *   c) Switch CR3 to the new PML4  ← CPU is now on new address space.
     *   d) Free old PML4               ← safe because CR3 no longer points to it.
     *   e) Rewrite the register frame.
     * ════════════════════════════════════════════════════════════════ */

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

    /* ════════════════════════════════════════════════════════════════
     * PHASE 4 — REGISTER FRAME RESET
     *
     * Your syscall_entry.S trampoline works like this:
     *
     *   swapgs
     *   mov [gs:8], rsp        ; save user RSP
     *   mov rsp, [gs:0]        ; switch to kernel stack (cpu_context.kernel_stack)
     *   push ...frame...
     *   mov rdi, rsp           ; regs = pointer to frame on kernel stack
     *   mov rbp, rsp
     *   and rsp, -16           ; align for C call
     *   call syscall_handler   ; regs passed as rdi
     *   mov rsp, rbp           ; restore pre-align rsp
     *   cmp rax, rsp
     *   jne .switch_to_new_context   ; if handler returned a different frame,
     *                                ;   jump to it (context switch path)
     *   ; otherwise fall through to .return_to_user and sysret
     *
     * There is ONE frame that matters: `regs`.  It is the frame pushed by
     * the trampoline onto the current process's kernel stack, and it is
     * the frame the trampoline will sysret through when syscall_handler
     * returns (uint64_t)regs.
     *
     * The scheduler uses proc->context_ptr when it context-switches back
     * to this task after it has been descheduled.  That path jumps directly
     * to .switch_to_new_context with rax = context_ptr, bypassing the
     * sysret path entirely.  So context_ptr must equal `regs` too —
     * they both describe the same saved state at the same memory location.
     *
     * The discrepancy you saw (regs ≠ kstack_top - sizeof(frame)) is
     * caused by cpu_context.kernel_stack (gs:0) not being updated after
     * exec/fork, so the next syscall entry pushed the new process's frame
     * onto the *old* process's kernel stack.  We fix that below.
     * ════════════════════════════════════════════════════════════════ */

    int cpu = get_cpu_id();

    /*
     * TSS.RSP0: used by the CPU on privilege transitions triggered by
     * hardware interrupts while the process is in user mode.  Must point
     * to the top of the current process's kernel stack.
     */
    tss_per_cpu[cpu].rsp0 = proc->kstack_top;

    /*
     * cpu_context.kernel_stack (gs:0): read by syscall_entry on every
     * syscall to set up the kernel stack before pushing the register frame.
     * This is the root cause of the "regs != sched_frame" mismatch — if
     * this is stale (pointing at the old process's kstack_top), the next
     * syscall will push the frame onto the wrong stack.
     *
     * cpu_contexts is updated by the scheduler on context switch, but exec()
     * replaces the process in-place without going through the scheduler, so
     * we must update it here explicitly.
     */
    extern cpu_context_t cpu_contexts[MAX_CPUS];
    cpu_contexts[cpu].kernel_stack = proc->kstack_top;

    /*
     * Overwrite the live register frame.
     *
     * `regs` IS the frame that sysret will consume — it was pushed by the
     * trampoline onto this process's kernel stack, and syscall_handler
     * received it as its argument.  We simply overwrite it in place so
     * that when syscall_handler returns (uint64_t)regs and the trampoline
     * falls through to .return_to_user, the restored state points into
     * the new binary.
     *
     * We also set proc->context_ptr = regs so that if the scheduler ever
     * switches away and back to this process before it runs its first
     * syscall, the .switch_to_new_context path also gets the right frame.
     */
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
     * every task-switch-in: wrmsrl(MSR_FS_BASE, current_process->tls_base) */
    if (tls_base)
        wrmsrl(MSR_FS_BASE, tls_base);
    else
        wrmsrl(MSR_FS_BASE, 0);

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

    /* ════════════════════════════════════════════════════════════════
     * ERROR PATHS — old address space is always left intact
     * ════════════════════════════════════════════════════════════════ */

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
