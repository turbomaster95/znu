#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>

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

            // Restore context
            memcpy(regs, &current_process->context, sizeof(registers_t));
        }
    }
}
