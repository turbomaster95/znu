#include <proc.h>
#include <page.h>
#include <string.h>
#include <stdlib.h>

extern uint64_t* kernel_pml4;
extern uint64_t get_timer_ticks(void);
extern void force_context_restore(registers_t* regs) __attribute__((noreturn));

static void kthread_trampoline(void)
{
    process_t* me = current_process;
    void (*fn)(void*) = (void (*)(void*))me->entry;
    void* arg = (void*)me->stack_top;

    __asm__ volatile("sti");

    fn(arg);

    kthread_exit(0);
    while (1) __asm__ volatile("hlt");
}

process_t* kthread_create(void (*fn)(void*), void* arg, const char* name)
{
    process_t* t = kzalloc(sizeof(process_t));
    if (!t) return NULL;

    void* kstack = kmalloc(KTHREAD_STACK_SIZE);
    if (!kstack) { kfree(t); return NULL; }

    t->kstack_top = ((uintptr_t)kstack + KTHREAD_STACK_SIZE) & ~0xFULL;
    t->pid = next_pid++;
    t->parent_pid = 0;
    t->state = TASK_READY;
    t->priority = PRIO_NORMAL;
    t->pml4 = kernel_pml4;
    t->is_kthread = true;

    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';

    t->entry = (uintptr_t)fn;
    t->stack_top = (uintptr_t)arg;

    registers_t* regs = (registers_t*)(t->kstack_top - sizeof(registers_t));
    memset(regs, 0, sizeof(registers_t));

    regs->rip = (uintptr_t)kthread_trampoline;
    regs->rsp = t->kstack_top;
    regs->cs = 0x08;      /* kernel code */
    regs->ss = 0x10;      /* kernel data */
    regs->ds = 0x10;
    regs->es = 0x10;
    regs->rflags = 0x202; /* IF=1 */

    t->context_ptr = regs;

    add_process(t);

    debugln("[kthread] Created '%s' pid=%llu", t->name, t->pid);
    return t;
}

void kthread_sleep_ms(uint64_t ms)
{
    if (!current_process || !current_process->is_kthread) return;

    current_process->sleep_deadline = get_timer_ticks() + ms;
    current_process->state = TASK_SLEEPING;

    kthread_yield();
}

void kthread_yield(void)
{
    __asm__ volatile("int $0x30");
}

void kthread_wake(process_t* thread)
{
    if (!thread || thread->state != TASK_SLEEPING) return;
    thread->sleep_deadline = 0;
    thread->state = TASK_READY;
}

void kthread_exit(int code)
{
    if (!current_process) return;

    current_process->state = TASK_ZOMBIE;
    current_process->exit_code = code;

    /* If parent is waiting, wake it */
    for (int i = 0; i < process_count; i++) {
        if (processes[i] && processes[i]->pid == current_process->parent_pid) {
            if (processes[i]->state == TASK_WAITING)
                processes[i]->state = TASK_READY;
            break;
        }
    }

    kthread_yield();
}
