# Znu

[<img src="https://raw.githubusercontent.com/turbomaster95/znu/refs/heads/main/Documentation/images/nori.png" width="250" align="right" alt="Nori" title="This is Znu's mascot, Nori!">]()

Znu is a modular, SMP-capable kernel and operating-system project, primarily targeting x86_64. It is written mainly in C with architecture-specific assembly and a small amount of supporting tooling.

The project started in April 2026 from a simple question:

> What if I read the OSDev Wiki and tried to make an operating system?

Znu has since grown into a kernel with its own memory management, scheduler, process model, VFS, TTY layer, syscall ABI, userspace C libraries, loadable kernel modules, device files, and networking system.

## What exists today

### Architecture and boot

- x86_64 architecture support
- Limine boot integration
- BIOS and UEFI ISO generation
- GDT and IDT setup
- x86 exception/interrupt entry stubs
- legacy PIC support
- local APIC support
- APIC timer
- SMP/AP startup and per-CPU state
- CPUID support
- TSC calibration
- RTC and PIT support
- x86 syscall entry
- ring-3/user-mode entry
- PCI enumeration
- ACPI through uACPI

### Memory management

- Limine memory-map based physical memory discovery
- HHDM-based physical/virtual address conversion
- buddy-based physical page allocation
- zeroed physical-page allocation
- page freeing
- 4 KiB page mapping
- 2 MiB huge-page mapping
- virtual-memory address translation
- kernel address-space construction
- user address-space cloning
- user page-table teardown
- slab allocation
- kernel heap allocation interfaces

### Processes and scheduling

- process table
- PID allocation
- process states
- priorities
- round-robin selection within the highest runnable priority
- sleeping and wake-up
- wait channels
- child waiting
- process exit/zombie state
- kernel threads
- context saving
- per-CPU kernel stacks
- `fork`
- `execve`
- `spawn`
- `brk`
- process address spaces
- basic signal state and signal delivery machinery

### VFS and filesystems

- in-memory VFS node/file abstractions
- regular files and directories
- file descriptors
- path lookup
- directory enumeration
- `stat`/`fstat`
- device nodes
- generated devfs nodes
- generated procfs nodes
- FAT32 support
- disk/block I/O plumbing
- AHCI driver support

The tree currently contains devfs nodes for `/dev/null`, `/dev/zero`, `/dev/fb0`, and an evdev input node, plus procfs nodes including configuration data and `kallsyms`.

### TTY and terminal

- multiple TTY objects
- canonical input buffering
- echo
- signal-enabled line discipline flags
- CR-to-NL input handling
- output post-processing
- `ioctl` handling for termios and window size
- blocking TTY reads
- TTY wait/wake integration with the scheduler
- `/dev/ttyN` device nodes
- PS/2 input path
- Flanterm-backed terminal output

Znu is therefore beyond a framebuffer-only console: the kernel has a real TTY layer and userspace programs can use the corresponding file descriptors. PTYs are not currently part of the documented implemented interface.

### Syscalls and userspace ABI

The current x86 syscall implementation contains handlers for:

- `read`
- `write`
- `open`
- `close`
- `getdents`
- `sysinfo`
- `stat`
- `fstat`
- `spawn`
- `brk`
- `fork`
- `execve`
- `ioctl`
- `mount`
- `getrandom`
- `sigreturn`
- `arch_prctl`
- `zfilt` (Custom zfilter syscall)

The userspace libc/ulibc tree also provides POSIX-like wrappers and C runtime support. Some libc functions are still stubs or compatibility shims; see [Documentation/userspace.md](Documentation/userspace.md).

### Executables and init

Znu contains:

- ELF structures and loading interfaces
- creation of processes from ELF images
- user address-space construction
- an initramfs loaded as a Limine module
- optional LZ4-framed initramfs decompression
- CPIO `newc` parsing
- `/bin/init` execution in ring 3
- a small userspace shell based on zline
- userspace test programs

The kernel also supports loading a kernel module named `init.ko` from the initramfs when present.

### Drivers and networking

Current driver/source areas include:

- PCI
- AHCI
- Bochs/BGA framebuffer
- Intel E1000
- PS/2
- RTC
- PIT
- serial output
- input/evdev

The networking tree implements Ethernet framing plus ARP, IPv4 handling, TCP, UDP/DNS-related code, and a small TCP socket-like connection API used by the kernel test suite.
As of now, Networking should be regarded as experimental.

### Kernel modules and symbols

The module subsystem has structures and APIs for:

- module loading/unloading
- module lookup
- aliases
- dependencies
- exported symbols
- module parameters
- module metadata
- module reference counting

Kernel symbol support also feeds the procfs `kallsyms` node.

### Build and configuration

Znu has a Linux-kernel-inspired build organization:

- top-level `Kconfig`
- top-level `Kbuild`
- `ZNUmakefile`
- per-subsystem Makefiles
- generated configuration headers
- `menuconfig`/configuration targets through the in-tree Kconfig machinery
- a bootstrap `build.sh`
- an in-tree host-tool build system
- Limine ISO generation
- compressed CPIO initramfs generation
- optional UEFI/BIOS/multi-boot images
- QEMU run target
- FAT32 image generation

See `Documentation/build.md` and the existing `Documentation/kbuild/` documentation.

## Repository layout

```text
.
├── arch/                 Architecture-specific code
│   └── x86/              x86/x86_64 implementation
├── drivers/              Device drivers and networking
├── include/              Kernel and ABI headers
├── init/                 Initial userspace program
├── kernel/               Core kernel subsystems
│   └── vfs/              devfs and procfs generators/nodes
├── mm/                   Physical/virtual memory management
├── lib/                  Kernel libc and userspace libc support
├── user/                 Userspace test/utilities
├── configs/              Build, ISO and sysroot configuration
├── scripts/              Build and code-generation helpers
├── tools/                Host tools used by the build
├── yuki/                 UEFI/Yuki-related experimental tooling
├── Documentation/        Project documentation
├── Kconfig               Top-level kernel configuration
├── Kbuild                Top-level build object lists
├── ZNUmakefile           Main Kbuild-style build logic
└── build.sh              Build/bootstrap entry point
```

## Building

The repository provides `build.sh` as the normal entry point.

```sh
./build.sh tools
./build.sh kernel
```

or:

```sh
./build.sh build
```

The build script bootstraps the repository's host tools when required and then invokes the in-tree `zngmake` toolchain.

The top-level make system also provides configuration and run targets. When ISO generation is enabled, it can produce BIOS, UEFI, or combined Limine images.

Please refer to [Documentation/build.md](Documentation/build.md) for more information on building Znu.

For the current QEMU target, the build system can also create a FAT32 disk image and launch QEMU with an E1000 NIC and AHCI-backed disk.

See:

- `Documentation/build.md`
- `Documentation/kbuild/kbuild.txt`
- `Documentation/kbuild/kconfig.txt`
- `Documentation/kbuild/kconfig-language.txt`
- `Documentation/kbuild/makefiles.txt`

## Configuration

Important top-level configuration switches include:

- `CLANG`
- `CROSS_COMPILE`
- `GENERATE_ISO`
- `ISO_MULTI`
- `ISO_BIOS`
- `ISO_UEFI`
- `SMP`
- `MODULES`
- `KTEST`
- `BGA`
- `E1000`
- `AHCI`

The checked-in default configuration is under `configs/defconfig`.

## Documentation map

- `Documentation/README.md` - documentation index
- `Documentation/architecture.md` - x86_64 architecture and boot path
- `Documentation/memory.md` - PMM, VMM, HHDM and kernel allocation
- `Documentation/processes.md` - processes, scheduler, threads and signals
- `Documentation/vfs.md` - VFS, devfs, procfs and files
- `Documentation/tty.md` - TTY, input and terminal architecture
- `Documentation/userspace.md` - init, ELF, syscalls and libc
- `Documentation/drivers.md` - drivers and networking
- `Documentation/build.md` - building, configuration and QEMU
- `Documentation/status.md` - source-tree implementation checklist
- `Documentation/roadmap.md` - kernel development roadmap
- `Documentation/kbuild/` - Kbuild/Kconfig documentation
- `Documentation/cpuid.txt` - cpuid.h Documentation
- `Documentation/images/` - project logos
- `Documentation/stats/` - language statistics

## License

This Project is licensed under the Nicense 1.1.
See [LICENSE](LICENSE) and [NOTICE](NOTICE).

## Used Languages

![Languages Stat](./Documentation/stats/stats.png)
