# Userspace

## Init process

`init/main.c` is built as the initial userspace program.

The kernel loads `/bin/init` from the initramfs and creates a process from its ELF image.

The initial environment includes:

```text
PATH=/bin
TERM=linux
HOME=/
```

The init program uses the userspace C library and zline-based command-line editing/completion.

## Shell

The init program contains a small shell with built-in command handling and program spawning.

It includes these commands:

```text
help
ls
cat
reboot
shutdown
touch
clear
mem
echo
exit
mount
rand
```

Programs can be searched in the current directory and standard `/bin` and `/sbin` locations.

## ELF

`include/elf.h` contains ELF definitions and the kernel exposes loader interfaces including:

- `load_elf`
- `vmm_create_user_pml4`
- `create_process_from_elf`

The ELF path creates a user address space and prepares the process for ring-3 entry.

## Syscall ABI

The x86 syscall entry uses the processor's `syscall` instruction mechanism.

Current kernel syscall handlers include:

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

The exact numeric ABI is defined in the [linux_syscall_64.tbl](linux_syscall_64.tbl) file. Please refer to [Kernel Status](status.md) for more information.

## ulibc

`lib/ulibc/` contains the userspace C runtime/library layer.

It includes:

- startup code
- malloc
- POSIX-like wrappers
- string functions
- stdio
- math
- qsort
- setjmp
- signal interfaces
- stack checking

The library also integrates zline for the initial shell.

## Kernel libc

`lib/libc/` contains kernel-side C library functionality such as:

- string
- stdio
- stdlib
- TTY support
- ctype
- ELF helpers
- filesystem helpers
- math
- MD5

This is separate from the userspace `lib/ulibc` tree.

## User programs

The current `user/` tree contains small programs including:

- `hello`
- `poweroff`
- `test_shell`
- `ls`
- `shz`
- `fuzz`
- `ttytest`
- `evtest`
- `argt`
- `spin`

## Initramfs integration

The top-level build system populates `configs/sysroot`, creates a CPIO archive and compresses it with LZ4.

That archive is placed into the Limine ISO root and becomes the source of the initial VFS contents.

## libc completeness

The userspace library intentionally contains compatibility implementations and stubs. For example, several POSIX interfaces in `lib/ulibc/posix.c` return placeholder values or `-1`.

Therefore:

> Having a libc symbol does not imply that the corresponding kernel feature is fully implemented.

This is particularly important when testing software ported from Linux.
