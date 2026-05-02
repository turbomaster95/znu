#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>
#include <syscall.h>
#include <gdt.h>

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

    // If we only have one process and it's already running, just return
    if (process_count == 1 && current_process != NULL && current_process->state == TASK_RUNNING) {
        return regs;
    }

    // Save current context
    if (current_process != NULL) {
        current_process->context_ptr = regs;
        if (current_process->state == TASK_RUNNING) {
            current_process->state = TASK_READY;
        }
    }

    // Round-robin selection
    int next_index = (current_process_index + 1) % process_count;
    
    // Simple loop to find a ready process
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
            // Save current SSE state if any
            if (current_process) {
                __asm__ volatile("fxsave %0" : "=m"(current_process->sse_state));
            }

            // Switch address space only if different
            if (current_process == NULL || next_proc->pml4 != current_process->pml4) {
                vmm_switch(next_proc->pml4);
            }

            current_process_index = found;
            current_process = next_proc;
            current_process->state = TASK_RUNNING;

            // Restore next SSE state
            __asm__ volatile("fxrstor %0" : : "m"(current_process->sse_state));

            // Update the kernel stack pointer for the next interrupt from Ring 3
            extern struct tss kernel_tss;
            kernel_tss.rsp0 = current_process->kstack_top;

            // Update the kernel stack pointer in GS for the next SYSCALL
            extern cpu_context_t main_cpu_context;
            main_cpu_context.kernel_stack = current_process->kstack_top;

            // Return new context pointer
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
                        processes[i]->parent_pid = 0; // Prevent finding it again
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
    debugln("[proc] PID %d exiting with code %d", current_process->pid, code);
    current_process->state = TASK_ZOMBIE;
    current_process->exit_code = code;
    
    // Wake up parent
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
    
    // Clone address space
    extern uint64_t* vmm_clone_pml4(uint64_t* src_pml4);
    dst->pml4 = vmm_clone_pml4(src->pml4);
    if (!dst->pml4) {
        kfree(dst);
        return NULL;
    }
    
    // Allocate new kernel stack
    void* kstack = kmalloc(32768);
    dst->kstack_top = (uintptr_t)kstack + 32768;
    
    // We need to copy the kernel stack content where the syscall registers are
    // The syscall_entry.S pushed everything onto the stack.
    // The regs pointer points to the registers_t structure on the current kernel stack.
    // We want the child to have its own copy of those registers on its own kernel stack.
    
    // Calculate the offset of regs relative to the top of the parent's kernel stack
    uintptr_t offset = src->kstack_top - (uintptr_t)regs;
    
    // The child's context_ptr should be at the same offset on its own stack
    dst->context_ptr = (registers_t*)(dst->kstack_top - offset);
    
    // Copy the registers and everything below them (down to the current rsp)
    // But since we are IN the syscall, we only care about the registers_t frame.
    memcpy(dst->context_ptr, regs, offset);
    
    // Child returns 0
    dst->context_ptr->rax = 0;
    
    dst->state = TASK_READY;
    
    // Reset some process-specific fields if necessary
    // (e.g. child doesn't share same file handles in a simple way for now)
    
    return dst;
}
