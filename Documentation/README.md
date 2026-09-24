# Znu Documentation

This directory documents the **current implementation**. For a kernel development roadmap, refer to [roadmap.md](roadmap.md).

## Core

| Document | Covers |
|---|---|
| [architecture.md](architecture.md) | x86_64 boot, CPU setup, interrupts, APIC, SMP, ACPI |
| [memory.md](memory.md) | PMM, VMM, HHDM, pages, slab and heap |
| [processes.md](processes.md) | processes, scheduler, kernel threads and signals |
| [vfs.md](vfs.md) | VFS, file descriptors, devfs, procfs and FAT32 |
| [tty.md](tty.md) | TTY line discipline, terminal output and input |
| [userspace.md](userspace.md) | ELF, init, syscalls, libc and user programs |
| [drivers.md](drivers.md) | hardware drivers and networking |
| [build.md](build.md) | build system, configuration and QEMU |
| [status.md](status.md) | source-tree implementation inventory |
| [roadmap.md](roadmap.md) | kernel development roadmap |

## Documentation Cheatsheet

Descriptions in the documents distinguish between:

- **Implemented** - source code exists and is wired into the kernel/userspace path.
- **Experimental** - source code exists and is exercised by tests or initialization, but the subsystem is not yet a stable ABI.
- **Partial/stubbed** - an interface exists, but some operations are placeholders or incomplete.

a function existing in a header is not treated as proof that a complete subsystem exists.
