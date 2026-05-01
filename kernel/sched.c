#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>
#include <syscall.h>

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

void scheduler(registers_t* regs) {
    if (process_count == 0) return;

    // If we only have one process and it's already running, just return
    if (process_count == 1 && current_process != NULL && current_process->state == TASK_RUNNING) {
        return;
    }

    // Save current context
    if (current_process != NULL) {
        memcpy(&current_process->context, regs, sizeof(registers_t));
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
        if (processes[idx]->state == TASK_READY || processes[idx]->state == TASK_RUNNING) {
            found = idx;
            break;
        }
    }

    if (found != -1) {
        process_t* next_proc = processes[found];
        
        // Only switch if it's a different process or not running
        if (next_proc != current_process || next_proc->state != TASK_RUNNING) {
            // Switch address space only if different
            if (current_process == NULL || next_proc->pml4 != current_process->pml4) {
                vmm_switch(next_proc->pml4);
            }

            current_process_index = found;
            current_process = next_proc;
            current_process->state = TASK_RUNNING;

            // Update the kernel stack pointer in GS for the next SYSCALL
            extern cpu_context_t main_cpu_context;
            main_cpu_context.kernel_stack = current_process->kstack_top;

            // Restore context
            memcpy(regs, &current_process->context, sizeof(registers_t));
        }
    }
}

int do_wait(int pid) {
    while (1) {
        int found_idx = -1;
        for (int i = 0; i < process_count; i++) {
            if (processes[i]->parent_pid == current_process->pid) {
                if (pid == -1 || processes[i]->pid == (uint64_t)pid) {
                    if (processes[i]->state == TASK_ZOMBIE) {
                        int code = processes[i]->exit_code;
                        processes[i]->parent_pid = 0; // Prevent finding it again
                        return code;
                    }
                    found_idx = i;
                }
            }
        }
        
        if (found_idx == -1) return -1; // No such child
        
        current_process->state = TASK_WAITING;
        __asm__ volatile("sti; hlt; cli");
    }
}

void do_exit(int code) {
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
