#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>
#include <syscall.h>
#include <gdt.h>
#include <lapic.h>

extern void force_context_restore(registers_t* regs) __attribute__((noreturn));
extern uint64_t next_pid;
extern uint64_t* vmm_clone_pml4(uint64_t* src_pml4);
extern void vmm_switch(uint64_t* pml4);
extern vfs_file_t* dup_file(vfs_file_t* src_file);

process_t* processes[MAX_PROCESSES];
int process_count = 0;
int current_process_index = -1;

process_t* current_process = NULL;
process_t* init_process = NULL;
process_t* idle_process = NULL;

static int rr_cursor = 0;

static int process_table_find_free_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!processes[i])
            return i;
    }

    return -1;
}

static int process_table_find(process_t* proc) {
    if (!proc)
        return -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i] == proc)
            return i;
    }

    return -1;
}

static process_t* find_process_by_pid(uint64_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i] && processes[i]->pid == pid)
            return processes[i];
    }

    return NULL;
}

static void process_remove(process_t* proc) {
    int index = process_table_find(proc);

    if (index < 0)
        return;

    processes[index] = NULL;

    if (process_count > 0)
        process_count--;

    if (current_process_index == index)
        current_process_index = -1;
}

static void process_save_context(process_t* proc, registers_t* regs) {
    if (!proc || !regs)
        return;

    memcpy(&proc->context, regs, sizeof(registers_t));
    proc->context_ptr = &proc->context;
}

static bool process_is_runnable(process_t* proc) {
    if (!proc || proc == idle_process)
        return false;

    return proc->state == TASK_READY ||
           proc->state == TASK_RUNNING;
}

static void wake_sleeping_processes(void) {
    uint64_t now = get_timer_ticks();

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = processes[i];

        if (!proc || proc->state != TASK_SLEEPING)
            continue;

        if (proc->sleep_deadline &&
            now >= proc->sleep_deadline) {
            proc->sleep_deadline = 0;
            proc->state = TASK_READY;
        }
    }
}

static int find_highest_priority(void) {
    int highest = -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = processes[i];

        if (!process_is_runnable(proc))
            continue;

        if ((int)proc->priority > highest)
            highest = (int)proc->priority;
    }

    return highest;
}

static int pick_next_process(void) {
    int highest = find_highest_priority();

    if (highest < 0)
        return -1;

    if (rr_cursor < 0 || rr_cursor >= MAX_PROCESSES)
        rr_cursor = 0;

    for (int n = 0; n < MAX_PROCESSES; n++) {
        int index = (rr_cursor + n) % MAX_PROCESSES;
        process_t* proc = processes[index];

        if (!process_is_runnable(proc))
            continue;

        if ((int)proc->priority != highest)
            continue;

        return index;
    }

    return -1;
}

static void update_kernel_stack(process_t* proc) {
    if (!proc)
        return;

    int cpu_id = get_cpu_id();

    tss_per_cpu[cpu_id].rsp0 = proc->kstack_top;

    extern cpu_context_t cpu_contexts[MAX_CPUS];
    cpu_contexts[cpu_id].kernel_stack = proc->kstack_top;
}

static void process_close_files(process_t* proc) {
    if (!proc)
        return;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!proc->files[i])
            continue;

        kfree(proc->files[i]);
        proc->files[i] = NULL;
    }
}

static void process_destroy(process_t* proc) {
    if (!proc || proc == idle_process)
        return;

    process_close_files(proc);

    if (proc->kstack_top) {
        uintptr_t stack_base = proc->kstack_top - 32768;
        kfree((void*)stack_base);
        proc->kstack_top = 0;
    }

    if (proc->pml4 && !proc->is_kthread) {
        vmm_free_user_pages(proc->pml4);
        proc->pml4 = NULL;
    }

    kfree(proc);
}

static void adopt_children(process_t* proc) {
    if (!proc || !init_process || proc == init_process)
        return;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* child = processes[i];

        if (!child || child->parent_pid != proc->pid)
            continue;

        child->parent_pid = init_process->pid;

        if (child->state == TASK_ZOMBIE &&
            init_process->state == TASK_WAITING) {
            init_process->state = TASK_READY;
        }
    }
}

void sys_yield(void) {
    __asm__ volatile("int $0x30");
}

void kernel_idle_loop(void) {
    while (1)
        __asm__ volatile("sti; hlt");
}

void init_scheduler(void) {
    memset(processes, 0, sizeof(processes));

    process_count = 0;
    current_process_index = -1;
    current_process = NULL;
    init_process = NULL;
    idle_process = NULL;
    rr_cursor = 0;

    idle_process = kzalloc(sizeof(process_t));

    if (!idle_process)
        panic("unable to allocate idle process");

    idle_process->pid = 0;
    idle_process->parent_pid = 0;
    idle_process->state = TASK_RUNNING;
    idle_process->priority = PRIO_IDLE;
    idle_process->is_kthread = true;
    idle_process->pml4 = vmm_get_kernel_pml4();

    void* kstack = kmalloc(32768);

    if (!kstack)
        panic("unable to allocate idle process stack");

    idle_process->kstack_top = (uintptr_t)kstack + 32768;

    memset(&idle_process->context, 0, sizeof(registers_t));

    idle_process->context.es = 0x10;
    idle_process->context.ds = 0x10;
    idle_process->context.rip = (uintptr_t)kernel_idle_loop;
    idle_process->context.rsp = idle_process->kstack_top;
    idle_process->context.cs = 0x08;
    idle_process->context.ss = 0x10;
    idle_process->context.rflags = 0x202;

    idle_process->context_ptr = &idle_process->context;

    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : "=m"(idle_process->sse_state));
}

int add_process(process_t* proc) {
    if (!proc)
        return -1;

    if (process_table_find(proc) >= 0)
        return 0;

    int slot = process_table_find_free_slot();

    if (slot < 0) {
        debugerr("[sched] process table full");
        return -1;
    }

    processes[slot] = proc;
    process_count++;

    return 0;
}

registers_t* scheduler(registers_t* regs) {
    process_t* old_process = current_process;

    if (old_process && regs)
        process_save_context(old_process, regs);

    wake_sleeping_processes();

    if (old_process &&
        old_process->state == TASK_RUNNING &&
        regs) {
        old_process->state = TASK_READY;
    }

    int next_index = pick_next_process();

    if (next_index < 0) {
        if (old_process &&
            process_is_runnable(old_process)) {
            old_process->state = TASK_RUNNING;
            return regs;
        }

        if (!old_process && regs)
            return regs;

        if (idle_process) {
            current_process = idle_process;
            current_process_index = -1;

            update_kernel_stack(idle_process);

            return idle_process->context_ptr;
        }

        if (regs)
            return regs;

        panic("[sched] no context available");
    }

    process_t* next = processes[next_index];

    if (!next) {
        if (regs)
            return regs;

        if (idle_process)
            return idle_process->context_ptr;

        panic("[sched] selected process disappeared");
    }

    if (next == old_process) {
        next->state = TASK_RUNNING;
        current_process_index = next_index;
        rr_cursor = (next_index + 1) % MAX_PROCESSES;
        return regs;
    }

    if (old_process && old_process != idle_process) {
        __asm__ volatile("fxsave %0" : "=m"(old_process->sse_state));
    }

    if (!old_process || old_process->pml4 != next->pml4)
        vmm_switch(next->pml4);

    current_process = next;
    current_process_index = next_index;
    next->state = TASK_RUNNING;
    rr_cursor = (next_index + 1) % MAX_PROCESSES;

    update_kernel_stack(next);

    __asm__ volatile("fxrstor %0" : : "m"(next->sse_state));

    debugln("[sched] switch %llu -> %llu",
            old_process ? old_process->pid : 0,
            next->pid);

    return &next->context;
}

int do_wait(int pid, int* status, bool* should_block) {
    if (!current_process || !should_block)
        return -1;

    *should_block = false;

    bool has_children = false;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* child = processes[i];

        if (!child)
            continue;

        if (child->parent_pid != current_process->pid)
            continue;

        if (pid != -1 && child->pid != (uint64_t)pid)
            continue;

        has_children = true;

        if (child->state != TASK_ZOMBIE)
            continue;

        int child_pid = (int)child->pid;

        if (status)
            *status = child->exit_code;

        process_remove(child);
        process_destroy(child);

        return child_pid;
    }

    if (!has_children)
        return -1;

    current_process->state = TASK_WAITING;
    *should_block = true;

    return 0;
}

registers_t* do_exit(int code) {
    if (!current_process || current_process == idle_process)
        panic("invalid process exit");

    process_t* exiting = current_process;

    exiting->exit_code = code;
    exiting->state = TASK_ZOMBIE;

    adopt_children(exiting);

    process_t* parent =
        find_process_by_pid(exiting->parent_pid);

    if (parent && parent->state == TASK_WAITING)
        parent->state = TASK_READY;

    current_process = NULL;
    current_process_index = -1;

    registers_t* next = scheduler(NULL);

    if (!next)
        panic("no runnable process after exit");

    force_context_restore(next);
}

process_t* clone_process(process_t* src, registers_t* regs) {
    if (!src || !regs)
        return NULL;

    if (src->state == TASK_ZOMBIE)
        return NULL;

    if (process_table_find_free_slot() < 0)
        return NULL;

    process_t* dst = kzalloc(sizeof(process_t));

    if (!dst)
        return NULL;

    dst->pid = next_pid++;
    dst->parent_pid = src->pid;
    dst->priority = src->priority;
    dst->state = TASK_READY;
    dst->is_kthread = false;

    dst->entry = src->entry;
    dst->stack_top = src->stack_top;
    dst->brk = src->brk;
    dst->brk_start = src->brk_start;
    dst->tls_base = src->tls_base;
    dst->tls_size = src->tls_size;

    dst->blocked_signals = src->blocked_signals;

    memcpy(dst->signal_handlers,
           src->signal_handlers,
           sizeof(dst->signal_handlers));

    memcpy(dst->name,
           src->name,
           sizeof(dst->name));

    dst->pml4 = vmm_clone_pml4(src->pml4);

    if (!dst->pml4) {
        kfree(dst);
        return NULL;
    }

    void* kstack = kmalloc(32768);

    if (!kstack) {
        vmm_free_user_pages(dst->pml4);
        kfree(dst->pml4);
        kfree(dst);
        return NULL;
    }

    dst->kstack_top = (uintptr_t)kstack + 32768;

    memcpy(&dst->context, regs, sizeof(registers_t));

    dst->context.rax = 0;
    dst->context_ptr = &dst->context;

    memcpy(dst->sse_state,
           src->sse_state,
           sizeof(dst->sse_state));

    for (int i = 0; i < MAX_FILES; i++) {
        if (!src->files[i])
            continue;

        dst->files[i] = dup_file(src->files[i]);

        if (!dst->files[i]) {
            for (int j = 0; j < MAX_FILES; j++) {
                if (dst->files[j]) {
                    kfree(dst->files[j]);
                    dst->files[j] = NULL;
                }
            }

            kfree(kstack);
            vmm_free_user_pages(dst->pml4);
            kfree(dst->pml4);
            kfree(dst);
            return NULL;
        }
    }

    if (add_process(dst) < 0) {
        for (int i = 0; i < MAX_FILES; i++) {
            if (dst->files[i]) {
                kfree(dst->files[i]);
                dst->files[i] = NULL;
            }
        }

        kfree(kstack);
        vmm_free_user_pages(dst->pml4);
        kfree(dst->pml4);
        kfree(dst);
        return NULL;
    }

    debugln("[clone] parent=%llu child=%llu",
            src->pid,
            dst->pid);

    return dst;
}
