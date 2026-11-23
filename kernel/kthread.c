#include <proc.h>
#include <page.h>
#include <string.h>
#include <stdlib.h>
#include <regs.h>
#include <gdt.h>

extern uint64_t* kernel_pml4;
extern uint64_t get_timer_ticks(void);
extern void vmm_switch(uint64_t* pml4);
extern int get_cpu_id(void);
extern cpu_context_t cpu_contexts[MAX_CPUS];

#define MAX_KTHREADS 64

static process_t* kthreads[MAX_KTHREADS];
static int kthread_count = 0;
static int kthread_cursor = 0;
static process_t bootstrap_thread;
static bool bootstrap_captured = false;

static int kthread_find_slot(void) {
    for (int i = 0; i < MAX_KTHREADS; i++) if (!kthreads[i]) return i;
    return -1;
}

static int kthread_find(process_t* thread) {
    for (int i = 0; i < MAX_KTHREADS; i++) if (kthreads[i] == thread) return i;
    return -1;
}

static bool kthread_runnable(process_t* thread) {
    return thread && thread->state == TASK_READY;
}

static void kthread_wake_sleepers(void) {
    uint64_t now = get_timer_ticks();
    for (int i = 0; i < MAX_KTHREADS; i++) {
        process_t* thread = kthreads[i];
        if (thread && thread->state == TASK_SLEEPING && thread->sleep_deadline && now >= thread->sleep_deadline) {
            thread->sleep_deadline = 0;
            thread->state = TASK_READY;
        }
    }
}

static process_t* kthread_pick_next(void) {
    int highest = -1;
    for (int i = 0; i < MAX_KTHREADS; i++) if (kthread_runnable(kthreads[i]) && (int)kthreads[i]->priority > highest) highest = (int)kthreads[i]->priority;
    if (highest < 0) return NULL;
    for (int n = 0; n < MAX_KTHREADS; n++) {
        int i = (kthread_cursor + n) % MAX_KTHREADS;
        if (kthread_runnable(kthreads[i]) && (int)kthreads[i]->priority == highest) {
            kthread_cursor = (i + 1) % MAX_KTHREADS;
            return kthreads[i];
        }
    }
    return NULL;
}

static void kthread_update_stack(process_t* thread) {
    if (!thread) return;
    int cpu = get_cpu_id();
    tss_per_cpu[cpu].rsp0 = thread->kstack_top;
    cpu_contexts[cpu].kernel_stack = thread->kstack_top;
}

static void kthread_trampoline(void) {
    process_t* me = current_process;
    if (!me) panic("[kthread] trampoline without current thread");
    void (*fn)(void*) = (void (*)(void*))me->entry;
    void* arg = (void*)me->stack_top;
    arch_enable_interrupts();
    fn(arg);
    kthread_exit(0);
    for (;;) arch_halt();
}

int kthread_register(process_t* thread) {
    if (!thread) return -1;
    if (kthread_find(thread) >= 0) return 0;
    int slot = kthread_find_slot();
    if (slot < 0) {
        debugerr("[kthread] thread table full");
        return -1;
    }
    kthreads[slot] = thread;
    kthread_count++;
    return 0;
}

process_t* kthread_create(void (*fn)(void*), void* arg, const char* name) {
    if (!fn) return NULL;
    process_t* thread = kzalloc(sizeof(process_t));
    if (!thread) return NULL;
    void* stack = kmalloc(KTHREAD_STACK_SIZE);
    if (!stack) {
        kfree(thread);
        return NULL;
    }
    thread->kstack_top = ((uintptr_t)stack + KTHREAD_STACK_SIZE) & ~0xFULL;
    thread->pid = next_pid++;
    thread->parent_pid = 0;
    thread->state = TASK_READY;
    thread->priority = PRIO_NORMAL;
    thread->pml4 = kernel_pml4;
    thread->is_kthread = true;
    thread->entry = (uintptr_t)fn;
    thread->stack_top = (uintptr_t)arg;
    strncpy(thread->name, name ? name : "kthread", sizeof(thread->name) - 1);
    thread->name[sizeof(thread->name) - 1] = '\0';
    memset(&thread->context, 0, sizeof(thread->context));
    thread->context.rip = (uintptr_t)kthread_trampoline;
    thread->context.rsp = thread->kstack_top;
    thread->context.cs = 0x08;
    thread->context.ss = 0x10;
    thread->context.ds = 0x10;
    thread->context.es = 0x10;
    thread->context.rflags = 0x202;
    thread->context_ptr = &thread->context;
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : "=m"(thread->sse_state));
    if (kthread_register(thread) < 0) {
        kfree(stack);
        kfree(thread);
        return NULL;
    }
    p_debug("kthread", "Created '%s' pid=%llu", thread->name, thread->pid);
    return thread;
}

registers_t* kthread_scheduler(registers_t* regs) {
    process_t* old = current_process;
    if (!bootstrap_captured && !old && regs) {
        memset(&bootstrap_thread, 0, sizeof(bootstrap_thread));
        bootstrap_thread.pid = UINT64_MAX;
        bootstrap_thread.state = TASK_READY;
        bootstrap_thread.priority = PRIO_HIGH;
        bootstrap_thread.is_kthread = true;
        bootstrap_thread.pml4 = vmm_get_kernel_pml4();
        memcpy(&bootstrap_thread.context, regs, sizeof(registers_t));
        bootstrap_thread.context_ptr = &bootstrap_thread.context;
        bootstrap_captured = true;
        old = &bootstrap_thread;
        current_process = old;
    } else if (old && regs) {
        memcpy(&old->context, regs, sizeof(registers_t));
        old->context_ptr = &old->context;
    }
    kthread_wake_sleepers();
    if (old && old != &bootstrap_thread && old->is_kthread && old->state == TASK_RUNNING) old->state = TASK_READY;
    if (old == &bootstrap_thread && old->state == TASK_RUNNING) old->state = TASK_READY;
    process_t* next = kthread_pick_next();
    if (!next && old && old->state == TASK_READY) {
        old->state = TASK_RUNNING;
        return regs ? regs : old->context_ptr;
    }
    if (!next) return regs;
    if (next == old) {
        next->state = TASK_RUNNING;
        return regs;
    }
    if (old) __asm__ volatile("fxsave %0" : "=m"(old->sse_state));
    if (!old || old->pml4 != next->pml4) vmm_switch(next->pml4);
    current_process = next;
    next->state = TASK_RUNNING;
    kthread_update_stack(next);
    __asm__ volatile("fxrstor %0" : : "m"(next->sse_state));
    return next->context_ptr;
}

void kthread_sleep_ms(uint64_t ms) {
    if (!current_process || !current_process->is_kthread || current_process == &bootstrap_thread) return;
    current_process->sleep_deadline = get_timer_ticks() + ms;
    current_process->state = TASK_SLEEPING;
    kthread_yield();
}

void kthread_yield(void) {
    __asm__ volatile("int $0x30");
}

void kthread_wake(process_t* thread) {
    if (!thread || !thread->is_kthread || thread->state != TASK_SLEEPING) return;
    thread->sleep_deadline = 0;
    thread->state = TASK_READY;
}

void kthread_exit(int code) {
    if (!current_process || !current_process->is_kthread || current_process == &bootstrap_thread) return;
    current_process->exit_code = code;
    current_process->state = TASK_ZOMBIE;
    kthread_yield();
    for (;;) arch_halt();
}

int kthread_get_count(void) {
    return kthread_count;
}


bool kthread_has_runnable(void) {
    kthread_wake_sleepers();

    for (int i = 0; i < MAX_KTHREADS; i++) {
        if (kthread_runnable(kthreads[i]))
            return true;
    }

    return false;
}
