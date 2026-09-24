# Implementation Status

This document is a source-tree inventory. It deliberately avoids treating every declaration, stub or experimental test as a finished subsystem.

## Architecture

| Area | Status | Source |
|---|---|---|
| x86_64 | Implemented | `arch/x86/` |
| GDT | Implemented | `arch/x86/gdt.c` |
| IDT | Implemented | `arch/x86/idt.c` |
| Exception/interrupt entry | Implemented | `arch/x86/idt_stub.S` |
| PIC | Implemented | `arch/x86/pic.c` |
| LAPIC | Implemented | `arch/x86/lapic.c` |
| SMP | Experimental/implemented | `kernel/smp.c`, `arch/x86/` |
| CPUID | Implemented | `arch/x86/include/cpuid.h`, `Documentation/cpuid.txt` |
| RTC | Implemented | `arch/x86/rtc.c` |
| PIT | Implemented | `arch/x86/pit.c` |
| TSC | Implemented | `arch/x86/tsc.c` |
| ACPI/uACPI | Integrated | `kernel/acpi.c` |
| x86 syscall entry | Implemented | `arch/x86/syscall_entry.S` |
| Ring-3 entry | Implemented | `arch/x86/usermode.S` |

## Memory

| Area | Status | Source |
|---|---|---|
| Physical memory discovery | Implemented | `mm/pmm.c` |
| Buddy PMM | Implemented | `mm/pmm.c`, `include/buddy.h` |
| HHDM | Implemented | `include/page.h`, `mm/` |
| 4 KiB mappings | Implemented | `mm/mappage.c` |
| 2 MiB mappings | Implemented | `mm/mappage.c`, `mm/vmm.c` |
| VMM | Implemented/experimental | `mm/vmm.c` |
| User address spaces | Implemented/experimental | `mm/vmm.c` |
| Slab allocator | Implemented | `mm/slab.c` |
| Kernel heap | Implemented | `mm/` |

## Processes

| Area | Status | Source |
|---|---|---|
| Process table | Implemented | `kernel/sched.c` |
| Priorities | Implemented | `include/proc.h`, `kernel/sched.c` |
| Round-robin scheduling | Implemented | `kernel/sched.c` |
| Sleeping/wakeup | Implemented | `kernel/sched.c` |
| TTY wait channel | Implemented | `kernel/tty.c`, `kernel/sched.c` |
| Kernel threads | Implemented/experimental | `kernel/kthread.c` |
| fork | Implemented/experimental | `arch/x86/syscall.c`, `kernel/` |
| execve | Implemented/experimental | `arch/x86/syscall.c`, `include/elf.h` |
| Signals | Partial/experimental | `kernel/signal.c` |
| TLS fields | Present | `include/proc.h` |

## VFS

| Area | Status | Source |
|---|---|---|
| VFS nodes/files | Implemented | `kernel/vfs/` |
| File descriptors | Implemented | `include/proc.h`, syscall layer |
| devfs | Implemented | `kernel/vfs/devfs/` |
| procfs | Implemented | `kernel/vfs/procfs/` |
| `/dev/null` | Implemented | `kernel/vfs/devfs/null/` |
| `/dev/zero` | Implemented | `kernel/vfs/devfs/zero/` |
| evdev | Implemented/experimental | `kernel/vfs/devfs/evdev/` |
| FAT32 | Implemented/experimental | `kernel/`, FAT32 headers |
| Mount syscall | Present/experimental | syscall layer |

## TTY

| Area | Status | Source |
|---|---|---|
| TTY objects | Implemented | `kernel/tty.c` |
| Canonical input | Implemented | `kernel/tty.c` |
| Echo | Implemented | `kernel/tty.c` |
| ICANON/ECHO/ISIG defaults | Implemented | `kernel/tty.c` |
| Blocking reads | Implemented | `kernel/tty.c` |
| O_NONBLOCK handling | Implemented | `kernel/tty.c` |
| termios ioctl | Implemented/partial | `kernel/tty.c` |
| Window size ioctl | Implemented | `kernel/tty.c` |
| `/dev/ttyN` | Implemented | `kernel/tty.c` |
| PTYs | Not Implemented | - |

## Userspace

| Area | Status | Source |
|---|---|---|
| ELF process creation | Implemented/experimental | `include/elf.h`, kernel loader |
| initramfs | Implemented | `kernel/main.c`, `kernel/lz4.c` |
| LZ4 initramfs decompression | Implemented | `kernel/lz4.c` |
| CPIO parsing | Implemented | kernel VFS/init path |
| `/bin/init` | Implemented | `init/main.c` |
| userspace C runtime | Implemented/partial | `lib/ulibc/` |
| userspace shell | Implemented/experimental | `init/main.c` |
| user test programs | Present | `user/` |

## Syscalls

The current kernel syscall source defines handlers for:

```text
read
write
open
close
getdents
sysinfo
stat
fstat
spawn
brk
fork
execve
ioctl
mount
getrandom
sigreturn
arch_prctl
zfilt
```

This is only implemented partially, not having full Linux/POSIX compatibility.

## Drivers

| Driver/subsystem | Status |
|---|---|
| PCI | Implemented |
| AHCI | Implemented/experimental |
| Bochs/BGA | Implemented/experimental |
| Intel E1000 | Implemented/experimental |
| PS/2 | Implemented |
| RTC | Implemented |
| PIT | Implemented |
| LAPIC | Implemented |
| Serial | Implemented |
| evdev | Implemented/experimental |

## Networking

| Feature | Status |
|---|---|
| Ethernet | Implemented/experimental |
| ARP | Implemented/experimental |
| IPv4 | Implemented/experimental |
| TCP | Implemented/experimental |
| UDP structures/path | Partial |
| DNS | Implemented/experimental |
| HTTP test | Test code present |
| UDP echo | Stubbed |

## Modules and symbols

| Area | Status |
|---|---|
| Loadable kernel modules | Implemented/experimental |
| Module metadata | Implemented |
| Module dependencies | Implemented in module model |
| Exported symbols | Implemented |
| Module parameters | Implemented in module model |
| kallsyms procfs node | Implemented |

