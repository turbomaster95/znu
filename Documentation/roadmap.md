# Znu Roadmap

This roadmap tracks the major work planned for Znu. New Items may get added or may move around as the kernel develops.

## 0. Kernel Foundation

- [ ] Stabilize early boot on BIOS and UEFI
- [ ] Clean up boot-time memory initialization
- [ ] Improve panic and kernel error reporting
- [ ] Add consistent kernel logging levels
- [ ] Add kernel assertions and debugging helpers
- [ ] Improve SMP startup and CPU hotplug foundations
- [ ] Audit interrupt handling across all architectures/components
- [ ] Audit locking and interrupt-safety rules
- [ ] Add kernel stack protection
- [ ] Add basic kernel stack traces
- [ ] Improve symbol resolution for panic traces

## 1. Memory Management

- [ ] Finish PMM edge-case handling
- [ ] Audit buddy allocator merging/splitting
- [ ] Improve slab allocator
- [ ] Add allocator statistics
- [ ] Add memory debugging facilities
- [ ] Add guard pages where practical
- [ ] Add page poisoning/debug mode
- [ ] Improve virtual address-space management
- [ ] Finish copy-on-write page handling
- [ ] Add lazy user-page allocation
- [ ] Add demand paging
- [ ] Add page-fault driven memory growth
- [ ] Improve `mmap`
- [ ] Add `munmap`
- [ ] Add `mprotect`
- [ ] Add shared mappings
- [ ] Add anonymous mappings
- [ ] Add memory-mapped files

## 2. Processes and Scheduling

- [ ] Stabilize process creation
- [ ] Stabilize `fork`
- [ ] Stabilize `execve`
- [ ] Stabilize `spawn`
- [ ] Finish process exit handling
- [ ] Finish zombie/reaping behavior
- [ ] Improve parent/child process tracking
- [ ] Finish wait primitives
- [ ] Improve scheduler locking
- [ ] Improve scheduler behavior on SMP
- [ ] Add CPU affinity
- [ ] Add per-CPU run queues
- [ ] Add scheduler statistics
- [ ] Add process priorities/niceness
- [ ] Add kernel thread management
- [ ] Add process groups
- [ ] Add sessions
- [ ] Add thread IDs
- [ ] Add thread-local storage support

## 3. Signals

- [ ] Stabilize signal delivery
- [ ] Finish signal masks
- [ ] Finish pending signal handling
- [ ] Finish `sigaction`
- [ ] Finish `sigprocmask`
- [ ] Finish `kill`
- [ ] Finish signal frame creation
- [ ] Stabilize `sigreturn`
- [ ] Add process-group signal delivery
- [ ] Add default signal actions
- [ ] Add signal interruption of blocking syscalls

## 4. TTY and Terminal

- [ ] Stabilize keyboard input
- [ ] Stabilize TTY input buffering
- [ ] Finish canonical input handling
- [ ] Finish raw input mode
- [ ] Finish echo modes
- [ ] Finish signal-generating control characters
- [ ] Implement proper terminal output processing
- [ ] Improve terminal resize handling
- [ ] Add terminal attributes
- [ ] Add controlling terminals
- [ ] Add session/foreground process-group handling
- [ ] Implement PTYs
- [ ] Add PTY master devices
- [ ] Add PTY slave devices
- [ ] Connect PTYs to the shell
- [ ] Add a virtual console layer
- [ ] Support multiple virtual terminals
- [ ] Add terminal switching
- [ ] Integrate Flanterm cleanly with the console/TTY layer

## 5. VFS

- [ ] Stabilize VFS path resolution
- [ ] Improve mount handling
- [ ] Improve file descriptor lifetime handling
- [ ] Finish directory iteration
- [ ] Improve `stat`/`fstat`
- [ ] Add file permission handling
- [ ] Add ownership and group metadata
- [ ] Add timestamps
- [ ] Add symbolic links
- [ ] Add hard links
- [ ] Add rename support
- [ ] Add unlink support
- [ ] Add `mkdir`
- [ ] Add `rmdir`
- [ ] Add `truncate`
- [ ] Add `dup`
- [ ] Add `dup2`
- [ ] Add `fcntl`
- [ ] Improve mount namespaces
- [ ] Add filesystem registration infrastructure
- [ ] Improve filesystem error handling

## 6. Filesystems

- [ ] Stabilize FAT32
- [ ] Improve FAT32 write support
- [ ] Add an in-memory filesystem
- [ ] Improve devfs
- [ ] Improve procfs
- [ ] Add sysfs-style device information
- [ ] Add a native Znu filesystem
- [ ] Add filesystem journaling
- [ ] Add filesystem consistency checking
- [ ] Add block-cache support

## 7. Block and Storage

- [ ] Stabilize AHCI
- [ ] Improve SATA error handling
- [ ] Add block-device abstraction cleanup
- [ ] Add partition discovery
- [ ] Add GPT support
- [ ] Add MBR support
- [ ] Add generic block cache
- [ ] Add asynchronous block I/O
- [ ] Add NVMe support
- [ ] Add virtio-blk support
- [ ] Add removable-media support

## 8. Device Model

- [ ] Define a consistent device model
- [ ] Improve driver registration
- [ ] Improve device lifetime management
- [ ] Add device dependency tracking
- [ ] Improve devfs integration
- [ ] Add device probing infrastructure
- [ ] Add driver matching
- [ ] Improve module/driver loading
- [ ] Add device tree/ACPI information where useful
- [ ] Improve hotplug support

## 9. PCI and Hardware

- [ ] Stabilize PCI enumeration
- [ ] Improve PCI BAR handling
- [ ] Add PCI capability parsing
- [ ] Add MSI support
- [ ] Add MSI-X support
- [ ] Improve interrupt routing
- [ ] Improve ACPI support
- [ ] Improve HPET support
- [ ] Improve RTC support
- [ ] Add watchdog support
- [ ] Add basic power-management support

## 10. Input

- [ ] Stabilize PS/2 keyboard
- [ ] Add PS/2 mouse
- [ ] Finish evdev interface
- [ ] Add USB HID support
- [ ] Add USB keyboard support
- [ ] Add USB mouse support
- [ ] Add input event abstraction
- [ ] Add input device discovery

## 11. Graphics and Display

- [ ] Stabilize framebuffer initialization
- [ ] Improve `/dev/fb0`
- [ ] Add framebuffer mode information
- [ ] Add framebuffer mmap support
- [ ] Improve console rendering
- [ ] Add cursor handling
- [ ] Add font handling
- [ ] Add basic graphics primitives
- [ ] Add a simple userspace framebuffer API
- [ ] Add VBE/UEFI framebuffer support cleanup
- [ ] Add VirtIO-GPU support
- [ ] Add basic DRM-like abstractions

## 12. Networking

- [ ] Stabilize Ethernet layer
- [ ] Stabilize ARP
- [ ] Improve IPv4
- [ ] Finish ICMP
- [ ] Finish UDP
- [ ] Finish TCP
- [ ] Improve TCP retransmission
- [ ] Improve TCP congestion handling
- [ ] Improve socket lifetime management
- [ ] Add socket syscalls
- [ ] Add `bind`
- [ ] Add `listen`
- [ ] Add `accept`
- [ ] Add `connect`
- [ ] Add `send`
- [ ] Add `recv`
- [ ] Add `sendto`
- [ ] Add `recvfrom`
- [ ] Add Unix-domain sockets
- [ ] Add loopback networking
- [ ] Add DHCP
- [ ] Improve DNS
- [ ] Add IPv6
- [ ] Add network interfaces through devfs/sysfs
- [ ] Add virtio-net
- [ ] Add additional NIC drivers

## 13. Userspace

- [ ] Stabilize `/bin/init`
- [ ] Improve zline
- [ ] Add command history
- [ ] Add command editing
- [ ] Add tab completion
- [ ] Add environment variables
- [ ] Add shell pipelines
- [ ] Add redirection
- [ ] Add background processes
- [ ] Add job control
- [ ] Add shell scripting
- [ ] Expand userspace libc
- [ ] Implement missing libc system-call wrappers
- [ ] Improve `malloc`
- [ ] Improve stdio
- [ ] Improve string/memory functions
- [ ] Add `pthread` foundations
- [ ] Add POSIX compatibility where practical

## 14. Syscalls and ABI

- [ ] Audit every existing syscall
- [ ] Stabilize syscall argument validation
- [ ] Improve user-pointer validation
- [ ] Improve syscall error handling
- [ ] Document the Znu syscall ABI
- [ ] Add missing filesystem syscalls
- [ ] Add missing process syscalls
- [ ] Add missing signal syscalls
- [ ] Add socket syscalls
- [ ] Add time-related syscalls
- [ ] Add polling interfaces
- [ ] Add `select`/`poll`/`epoll`-style interfaces
- [ ] Add `pipe`
- [ ] Add `dup`
- [ ] Add `fcntl`
- [ ] Add `uname`
- [ ] Add `getcwd`
- [ ] Add `chdir`
- [ ] Add `getuid`/`setuid` foundations
- [ ] Add capability/permission foundations

## 15. IPC

- [ ] Add anonymous pipes
- [ ] Add named pipes
- [ ] Add Unix-domain sockets
- [ ] Add shared memory
- [ ] Add event notifications
- [ ] Add futexes
- [ ] Add POSIX-style message queues
- [ ] Add semaphores
- [ ] Add robust synchronization primitives
- [ ] Audit IPC behavior across processes

## 16. Time

- [ ] Stabilize TSC calibration
- [ ] Improve monotonic clock
- [ ] Improve realtime clock
- [ ] Add high-resolution timers
- [ ] Add per-process timers
- [ ] Add timer file descriptors
- [ ] Add nanosleep
- [ ] Add interval timers
- [ ] Improve SMP timekeeping
- [ ] Handle CPU frequency/TSC invariance correctly

## 17. Modules

- [ ] Stabilize module loading
- [ ] Stabilize module unloading
- [ ] Improve dependency resolution
- [ ] Improve symbol exports
- [ ] Improve module reference counting
- [ ] Add module versioning
- [ ] Add module parameter parsing
- [ ] Improve module error reporting
- [ ] Add module signing infrastructure
- [ ] Document the module ABI

## 18. Security

- [ ] Audit user/kernel boundary
- [ ] Audit syscall pointer validation
- [ ] Audit filesystem permissions
- [ ] Add users and groups
- [ ] Add file ownership
- [ ] Add permission checks
- [ ] Add capability-based permissions
- [ ] Add secure module loading
- [ ] Add kernel address-space randomization foundations
- [ ] Add user address-space randomization
- [ ] Add stack canaries
- [ ] Add W^X enforcement
- [ ] Add NX enforcement everywhere applicable
- [ ] Add hardened heap options
- [ ] Add security-focused kernel configuration options

## 19. Debugging

- [ ] Improve panic output
- [ ] Add kernel backtraces
- [ ] Add symbolized stack traces
- [ ] Add process inspection
- [ ] Add thread inspection
- [ ] Add memory inspection tools
- [ ] Add `/proc` debugging information
- [ ] Add kernel tracing
- [ ] Add syscall tracing
- [ ] Add scheduler tracing
- [ ] Add interrupt tracing
- [ ] Add configurable debug logging
- [x] Improve QEMU/GDB debugging support

## 20. Build System

- [x] Stabilize Kbuild-style build system
- [x] Improve Kconfig handling
- [x] Add configuration validation
- [x] Improve incremental builds
- [x] Add dependency tracking
- [x] Improve cross-compilation support
- [x] Add reproducible-build support
- [x] Add debug/release configurations
- [x] Add automated ISO generation
- [x] Add automated initramfs generation
- [ ] Add CI builds
- [ ] Add automated QEMU boot tests

## 21. Testing

- [x] Add kernel unit-test infrastructure
- [ ] Add allocator tests
- [ ] Add VMM tests
- [ ] Add VFS tests
- [ ] Add filesystem tests
- [ ] Add syscall tests
- [ ] Add process tests
- [ ] Add signal tests
- [ ] Add TTY tests
- [ ] Add networking tests
- [ ] Add userspace integration tests
- [ ] Add boot smoke tests
- [ ] Add SMP tests
- [ ] Add regression tests for known crashes
- [ ] Run tests automatically in CI

## 22. Documentation

- [x] Document kernel architecture
- [x] Document boot sequence
- [x] Document memory management
- [x] Document process management
- [x] Document scheduling
- [x] Document the syscall ABI
- [x] Document VFS internals
- [x] Document filesystem interfaces
- [x] Document driver interfaces
- [x] Document networking
- [x] Document modules
- [x] Document userspace
- [x] Document kernel configuration
- [ ] Document the build system
- [ ] Add driver-development documentation
- [ ] Add userspace-development documentation
- [ ] Keep documentation synchronized with the source tree

## 23. Compatibility

- [ ] Define the supported x86_64 execution environment
- [ ] Improve Linux/POSIX compatibility where useful
- [ ] Add compatibility wrappers in userspace
- [ ] Port small Unix utilities
- [ ] Port a usable shell
- [ ] Port basic text utilities
- [ ] Port basic filesystem utilities
- [ ] Port networking utilities
- [ ] Build a larger userspace environment
- [ ] Establish a stable userspace ABI

## 24. Distribution

- [x] Create a standard Znu disk image
- [ ] Create an installable disk image
- [x] Add partitioning/install scripts
- [ ] Add a default filesystem layout
- [ ] Add default configuration files
- [ ] Add package/install tooling
- [ ] Add a basic package format
- [ ] Add package repository tooling
- [ ] Add system upgrade tooling
- [ ] Add release image generation

## 25. Long-Term

- [ ] Full USB stack
- [ ] NVMe
- [ ] IPv6
- [ ] Multiple filesystem implementations
- [ ] Full PTY support
- [ ] Mature job-control shell
- [ ] Mature userspace libc
- [ ] Stable userspace ABI
- [ ] Hardware-independent driver interfaces
- [ ] More complete ACPI support
- [ ] Power management
- [ ] Suspend/resume foundations
- [ ] Virtualization support
- [ ] Container/isolation primitives
- [ ] 64-bit userspace ecosystem
- [ ] Native Znu development environment
- [ ] Self-hosting toolchain
- [ ] Self-hosting userspace
- [ ] Self-hosting Znu development

---

# Milestones

## M0 - Stable Kernel Base

- [ ] Reliable boot
- [ ] Reliable interrupts
- [ ] Reliable SMP initialization
- [x] Stable PMM/VMM
- [ ] Stable scheduler
- [ ] Stable process creation
- [ ] Stable VFS
- [ ] Stable TTY
- [ ] Stable userspace execution
- [ ] Kernel panic/backtrace support

## M1 - Usable Userspace

- [ ] Stable `init`
- [ ] Usable shell
- [ ] Working pipes
- [ ] Working redirection
- [ ] Working process management
- [ ] Working signals
- [ ] Working filesystem utilities
- [ ] Working terminal job control
- [ ] Expanded libc

## M2 - Usable System

- [ ] Stable storage stack
- [ ] Stable networking stack
- [ ] Socket API
- [ ] DHCP
- [ ] DNS
- [ ] SSH-capable userspace
- [ ] USB input
- [ ] Multiple terminals
- [ ] PTYs

## M3 - Development System

- [ ] Compiler/toolchain support
- [ ] Debugger support
- [ ] Build tools
- [ ] Text editor
- [ ] Shell scripting
- [ ] Package tooling
- [ ] Native development environment

## M4 - Self-Hosting

- [ ] Native compiler/toolchain
- [ ] Native build system
- [ ] Native debugger
- [ ] Native userspace development tools
- [ ] Kernel can be rebuilt from Znu
- [ ] Userspace can be rebuilt from Znu

## M5 - Znu Release

- [ ] Stable userspace ABI
- [ ] Documented kernel interfaces
- [ ] Automated test suite
- [ ] Reproducible builds
- [ ] Installable image
- [ ] Release tooling
- [ ] Hardware compatibility documentation
- [ ] First stable Znu release
