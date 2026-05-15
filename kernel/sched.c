#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>
#include <syscall.h>
#include <gdt.h>
#include <lapic.h>

#define MAX_PROCESSES 64

process_t* processes[MAX_PROCESSES];
int process_count = 0;
int current_process_index = -1;

process_t* current_process = NULL;
process_t* init_process = NULL;

extern void vmm_switch(uint64_t* pml4);

void init_scheduler(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i] = NULL;
    }
}

void add_process(process_t* proc) {
    if (process_count < MAX_PROCESSES) {
        processes[process_count++] = proc;
    }
}

registers_t* scheduler(registers_t* regs) {
    if (process_count == 0) return regs;

    if (process_count == 1 && current_process != NULL && current_process->state == TASK_RUNNING) {
        return regs;
    }

    if (current_process != NULL) {
        current_process->context_ptr = regs;
        if (current_process->state == TASK_RUNNING) {
            current_process->state = TASK_READY;
        }
    }

    int next_index = (current_process_index + 1) % process_count;
    
    int found = -1;
    for (int i = 0; i < process_count; i++) {
        int idx = (next_index + i) % process_count;
        if (processes[idx] && (processes[idx]->state == TASK_READY || processes[idx]->state == TASK_RUNNING)) {
            found = idx;
            break;
        }
    }

    if (found != -1) {
        process_t* next_proc = processes[found];
        
        // Only switch if it's a different process or not running
        if (next_proc != current_process || next_proc->state != TASK_RUNNING) {
            if (current_process) {
                __asm__ volatile("fxsave %0" : "=m"(current_process->sse_state));
            }

            if (current_process == NULL || next_proc->pml4 != current_process->pml4) {
                vmm_switch(next_proc->pml4);
            }

            current_process_index = found;
            current_process = next_proc;
            current_process->state = TASK_RUNNING;

            __asm__ volatile("fxrstor %0" : : "m"(current_process->sse_state));

	    int cpu_id = get_cpu_id();
	    tss_per_cpu[cpu_id].rsp0 = current_process->kstack_top;
	    extern cpu_context_t cpu_contexts[MAX_CPUS];
	    cpu_contexts[cpu_id].kernel_stack = current_process->kstack_top;

            return current_process->context_ptr;
        }
    }
    return regs;
}

int do_wait(int pid, int* status) {
    while (1) {
        int found_idx = -1;
        for (int i = 0; i < process_count; i++) {
            if (!processes[i]) continue;
            if (processes[i]->parent_pid == current_process->pid) {
                if (pid == -1 || processes[i]->pid == (uint64_t)pid) {
                    if (processes[i]->state == TASK_ZOMBIE) {
                        int code = processes[i]->exit_code;
                        if (status) *status = code;
                        processes[i]->parent_pid = 0; 
                        return (int)processes[i]->pid; 
                    }
                    found_idx = i;
                }
            }
        }
        
        if (found_idx == -1) {
            return -1; 
        }
        
        current_process->state = TASK_WAITING;
        __asm__ volatile("sti; hlt; cli");
    }
}

void do_exit(int code) {
//    debugln("[proc] PID %d exiting with code %d", current_process->pid, code);
    current_process->state = TASK_ZOMBIE;
    current_process->exit_code = code;
    
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == current_process->parent_pid) {
            if (processes[i]->state == TASK_WAITING) {
                processes[i]->state = TASK_READY;
            }
            break;
        }
    }
    
    while(1) {
        __asm__ volatile("sti; hlt; cli");
    }
}

process_t* clone_process(process_t* src, registers_t* regs) {
    process_t* dst = kmalloc(sizeof(process_t));
    if (!dst) return NULL;
    
    memcpy(dst, src, sizeof(process_t));
    
    static uint64_t next_pid = 1000;
    dst->pid = next_pid++;
    dst->parent_pid = src->pid;
    
    extern uint64_t* vmm_clone_pml4(uint64_t* src_pml4);
    dst->pml4 = vmm_clone_pml4(src->pml4);
    if (!dst->pml4) {
        kfree(dst);
        return NULL;
    }
    
    // Allocate new kernel stack
    void* kstack = kmalloc(32768);
    dst->kstack_top = (uintptr_t)kstack + 32768;

    uintptr_t offset = src->kstack_top - (uintptr_t)regs;
    
    dst->context_ptr = (registers_t*)(dst->kstack_top - offset);
    
    // Copy the registers and everything below them (down to the current rsp)
    // But since we are IN the syscall, we only care about the registers_t frame.
    memcpy(dst->context_ptr, regs, offset);
    
    // Child returns 0
    dst->context_ptr->rax = 0;
    
    dst->state = TASK_READY;
    
    return dst;
}
