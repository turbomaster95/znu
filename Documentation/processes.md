# Processes, Scheduling and Signals

## Process model

The main process definition is in `include/proc.h`.

A process contains:

- PID and parent PID
- exit status
- PML4
- user entry point and stack
- kernel stack
- `brk` state
- saved CPU context
- scheduling state and priority
- wait state/channel
- file descriptors
- VFSE process state
- saved SSE state
- TLS metadata
- kernel-thread state
- sleep deadline
- pending/blocked signal masks
- signal handlers
- saved user context
- process name

The process table currently has a compile-time maximum of 64 processes and 32 file descriptors per process.

## States

The process state enum contains:

```text
TASK_RUNNING
TASK_READY
TASK_SLEEPING
TASK_WAITING
TASK_ZOMBIE
```

Wait reasons currently include child waits and TTY waits.

## Scheduler

`kernel/sched.c` maintains the process table and selects runnable processes.

The scheduler:

1. wakes sleeping processes whose deadlines have expired;
2. finds the highest runnable priority;
3. selects a runnable process of that priority using a round-robin cursor;
4. updates the current process and kernel stack state;
5. switches address spaces when necessary.

The implementation therefore combines priority selection with round-robin selection among equal-priority runnable processes.

## Kernel threads

`kernel/kthread.c` provides kernel-thread helpers, including creation, sleep, yielding, exit and wake-up.

Kernel threads use the same process/task infrastructure but are marked as kernel threads.

## Waiting

TTY reads can put the current process into `TASK_WAITING` with `WAIT_TTY`. Input wakes the waiting reader and returns it to the ready state.

Child waiting uses the corresponding process wait state/reason.

## fork and exec

The syscall layer exposes `fork` and `execve`.

The process/address-space code uses the VMM clone and ELF loading paths to create the child or replace a process image.

The current implementation is intentionally much smaller than Linux's process model and is a complete POSIX process ABI.

## Signals

The process structure contains pending and blocked signal masks plus 64 handler slots.

The kernel contains signal raising and delivery code, and the x86 syscall layer includes `sigreturn`.

The current signal implementation is therefore present but should be considered experimental rather than a complete POSIX signal subsystem.

## Kernel stacks and TSS

Each CPU has a kernel-stack context/TSS state. Scheduling a process updates the CPU's kernel stack pointer so a transition from userspace enters the kernel on the selected process's stack.
