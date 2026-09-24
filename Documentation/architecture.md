# Architecture and Boot

## Target

Znu currently has one architecture tree:

```text
arch/
└── x86/
```

The kernel is currently built for x86_64 and contains architecture-specific C and assembly.

## Early boot

Limine supplies the kernel with boot information including the memory map, framebuffer/modules and other boot data. Znu uses the Limine headers and boot protocol structures from `include/limine.h`.

The kernel entry eventually reaches `kmain()` in `kernel/main.c`.

The early initialization path includes:

1. LAPIC initialization.
2. Serial initialization.
3. PS/2 initialization.
4. RTC initialization.
5. Timekeeper initialization.
6. PIT setup.
7. LAPIC timer setup.
8. uACPI initialization.
9. Kernel initcalls.
10. Optional network and SMP initialization.
11. Disk and VFS initialization.
12. TTY initialization.
13. Syscall setup.
14. Initramfs decompression/parsing.
15. `/bin/init` ELF creation.
16. Switch to the init process address space.
17. Enter ring 3.

## CPU and descriptor tables

The x86 tree contains:

- `gdt.c`
- `gdt_flush.S`
- `idt.c`
- `idt_stub.S`
- `x86.S`
- `start.S`
- `panic.S`
- `usermode.S`

The GDT provides kernel/user segmentation state and the TSS-related kernel-stack setup used when entering the kernel from userspace.

The IDT and assembly stubs provide exception/interrupt entry.

## Interrupt controllers

The architecture tree contains both:

- legacy PIC support in `pic.c`
- local APIC support in `lapic.c`

The LAPIC timer is used by the scheduler/timekeeping path.

## Time

The x86 tree contains:

- RTC
- PIT
- TSC
- timekeeper
- CPU-speed calibration

The kernel initializes the RTC/timekeeper and arms the LAPIC periodic timer.

## SMP

SMP support is split between:

- `arch/x86/smp.h`
- `kernel/smp.c`
- LAPIC/IPI support
- per-CPU context/TSS state

The kernel also has a mailbox mechanism for sending worker tasks to other CPUs. The KTEST path exercises SMP worker dispatch when SMP is enabled.

## ACPI

Znu integrates uACPI through the kernel's uACPI glue layer. `kernel/acpi.c` and the uACPI initialization path are used during kernel startup.

## System calls

x86 syscall entry is implemented by:

- `arch/x86/syscall_entry.S`
- `arch/x86/syscall.c`
- `arch/x86/include/syscall.h`

The kernel configures the x86 syscall mechanism and enters the common syscall dispatcher from userspace.

## PCI and architecture-facing hardware

PCI enumeration is implemented under `arch/x86/pci.c` and is used by drivers such as E1000 and AHCI.

## Architecture boundary

Architecture-independent code should generally live in:

```text
kernel/
mm/
drivers/
lib/
```

while CPU-specific operations belong under `arch/x86/`.
