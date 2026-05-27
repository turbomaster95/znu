#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>
#include <syscall.h>
#include <gdt.h>
#include <lapic.h>

#define MAX_PROCESSES 64

extern void force_context_restore(registers_t* regs) __attribute__((noreturn));
extern uint64_t next_pid;
typedef struct vfs_file vfs_file_t; // Forward declare if not already fully visible
extern uint64_t* vmm_clone_pml4(uint64_t* src_pml4);
extern vfs_file_t* dup_file(vfs_file_t* src_file);

process_t* processes[MAX_PROCESSES];
int process_count = 0;
int current_process_index = -1;

process_t* current_process = NULL;
process_t* init_process = NULL;
process_t* idle_process = NULL;

extern void vmm_switch(uint64_t* pml4);

void sys_yield(void) {
    __asm__ volatile("int $0x30");
}

void kernel_idle_loop(void) {
    while (1) {
        __asm__ volatile("sti; hlt");
    }
}

void init_scheduler(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i] = NULL;
    }

    idle_process = kmalloc(sizeof(process_t));
    memset(idle_process, 0, sizeof(process_t));

    idle_process->pid = 0;
    idle_process->parent_pid = 0;
    idle_process->state = TASK_READY;
    idle_process->priority = PRIO_IDLE;

    void* kstack = kmalloc(32768);
    idle_process->kstack_top = (uintptr_t)kstack + 32768;

    idle_process->pml4 = vmm_get_kernel_pml4();

    idle_process->context_ptr = (registers_t*)(idle_process->kstack_top - sizeof(registers_t));
    memset(idle_process->context_ptr, 0, sizeof(registers_t));
    
    idle_process->context_ptr->rip = (uintptr_t)kernel_idle_loop;
    idle_process->context_ptr->cs = 0x08;
    idle_process->context_ptr->ss = 0x10;
    idle_process->context_ptr->rflags = 0x202; // Enabled interrupts
}

void add_process(process_t* proc) {
    if (process_count < MAX_PROCESSES) {
        processes[process_count++] = proc;
    }
}

registers_t* scheduler(registers_t* regs) {
    if (process_count == 0) return regs;

    if (current_process != NULL && regs != NULL) {
        current_process->context_ptr = regs;
        if (current_process->state == TASK_RUNNING) {
            current_process->state = TASK_READY;
        }
    }

    int best_index = -1;
    int highest_prio = -1;

    for (int i = 0; i < process_count; i++) {
        if (!processes[i]) continue;

        if (processes[i]->state == TASK_READY || processes[i]->state == TASK_RUNNING) {
            if ((int)processes[i]->priority > highest_prio) {
                highest_prio = (int)processes[i]->priority;
                best_index = i;
            }
        }
    }

    if (best_index == -1) {
        current_process = idle_process;
        current_process->state = TASK_RUNNING;

        int cpu_id = get_cpu_id();
        tss_per_cpu[cpu_id].rsp0 = current_process->kstack_top;
        extern cpu_context_t cpu_contexts[MAX_CPUS];
        cpu_contexts[cpu_id].kernel_stack = current_process->kstack_top;

        vmm_switch(current_process->pml4);
        return current_process->context_ptr;
    }

    process_t* next_proc = processes[best_index];

    if (next_proc != current_process || next_proc->state != TASK_RUNNING) {
        if (current_process && current_process != idle_process) {
            __asm__ volatile("fxsave %0" : "=m"(current_process->sse_state));
        }

        if (current_process == NULL || next_proc->pml4 != current_process->pml4) {
            vmm_switch(next_proc->pml4);
        }

        current_process_index = best_index;
        current_process = next_proc;
        current_process->state = TASK_RUNNING;

        __asm__ volatile("fxrstor %0" : : "m"(current_process->sse_state));

        int cpu_id = get_cpu_id();
        tss_per_cpu[cpu_id].rsp0 = current_process->kstack_top;
        extern cpu_context_t cpu_contexts[MAX_CPUS];
        cpu_contexts[cpu_id].kernel_stack = current_process->kstack_top;

        return current_process->context_ptr;
    }

    return regs;
}

int do_wait(int pid, int* status, bool* should_block) {
    bool has_children = false;
    *should_block = false;
    
    for (int i = 0; i < process_count; i++) {
        if (!processes[i]) continue;
        
        if (processes[i]->parent_pid == current_process->pid) {
            if (pid == -1 || processes[i]->pid == (uint64_t)pid) {
                has_children = true;
                
                if (processes[i]->state == TASK_ZOMBIE) {
                    int code = processes[i]->exit_code;
                    if (status) {
                        *status = code;
                    }
                    
                    int child_pid = (int)processes[i]->pid;
                    
                    kfree(processes[i]);
                    processes[i] = NULL;
                    
                    return child_pid; // Successfully reaped
                }
            }
        }
    }
    
    if (!has_children) {
        return -1; // No children to wait for
    }
    
    current_process->state = TASK_WAITING;
    *should_block = true;
    return 0;
}

registers_t* do_exit(int code) {
//    debugln("[proc] PID %d exiting with code %d", current_process->pid, code);
    
    current_process->state = TASK_ZOMBIE;
    current_process->exit_code = code;
    
    for (int i = 0; i < process_count; i++) {
        if (!processes[i]) continue;
        if (processes[i]->pid == current_process->parent_pid) {
            if (processes[i]->state == TASK_WAITING) {
                processes[i]->state = TASK_READY;
                //debugln("[proc] Woke up parent PID %d", processes[i]->pid);
            }
            break;
        }
    }
    
    registers_t* next_task_regs = scheduler(NULL);

    debugln("Attempting context switch: RIP=%p, RSP=%p, CS=%x", 
        next_task_regs->rip, next_task_regs->rsp, next_task_regs->cs);
    
    force_context_restore(next_task_regs);
}

process_t* clone_process(process_t* src, registers_t* regs) {
    process_t* dst = kmalloc(sizeof(process_t));
    if (!dst) {
        debugerr("[clone_process] kmalloc failed!");
        return NULL;
    }
    
    memset(dst, 0, sizeof(process_t));

    dst->pid = next_pid++;
    dst->parent_pid = src->pid;
    dst->priority = src->priority;
    dst->state = TASK_READY;

    dst->pml4 = vmm_clone_pml4(src->pml4);
    if (!dst->pml4) { kfree(dst); return NULL; }
    
    for(int i = 0; i < MAX_FILES; i++) {
        if(src->files[i]) {
            dst->files[i] = dup_file(src->files[i]); 
        }
    }

    void* kstack = kmalloc(32768);
    dst->kstack_top = ((uintptr_t)kstack + 32768) & ~0xF;

    dst->context_ptr = (registers_t*)(dst->kstack_top - sizeof(registers_t));
    memcpy(dst->context_ptr, regs, sizeof(registers_t));
    dst->context_ptr->rax = 0; 
    dst->context_ptr->rip = regs->rip;

    debugln("[clone] Source User RSP: %p", (void*)regs->rsp);
    debugln("[clone] Target Kernel RSP (Context): %p", (void*)dst->context_ptr->rsp);

    debugln("[clone] Child User RSP: %p", (void*)dst->context_ptr->rsp);

    return dst;
}
