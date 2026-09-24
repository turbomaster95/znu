# VFS and Filesystems

## VFS model

Znu's VFS represents filesystem objects with `vfs_node_t` objects and file descriptors with `vfs_file_t`.

Operations are provided through VFS operation tables. Filesystem/device-specific code can implement operations such as:

- read
- write
- readdir
- lookup/find
- other filesystem-specific hooks

The syscall layer uses this interface for path lookup and file operations.

## File descriptors

Processes have a fixed array of file pointers.

The current userspace-facing syscall layer implements:

- `read`
- `write`
- `open`
- `close`
- `getdents`
- `stat`
- `fstat`

`dup`/`dup2` wrappers also exist in ulibc.

## devfs

The devfs generator lives in:

```text
scripts/basic/devcom.c
```

and the node definitions are located in:

```text
kernel/vfs/devfs/
```

Device descriptions are generated from small per-device source files.

Current generated device areas include:

```text
/dev/null
/dev/zero
/dev/fb0
/dev/input/event1
/dev/tty0
/dev/tty1
...
```

The exact TTY count is controlled by the TTY implementation.

`/dev/null` discards writes and immediately reports EOF on reads.

`/dev/zero` returns zero-filled data.

The evdev node exposes queued `struct input_event` records.

## procfs

The procfs generator lives in:

```text
scripts/basic/devcom.c
```

and the node definitions are located in:

```text
kernel/vfs/procfs/
```

Current source-tree nodes include:

- `config` (`/proc/config`)
- `kallsyms` (`/proc/kallsyms`)

`config` exposes the generated kernel configuration data.

`kallsyms` exposes kernel symbol information through the symbol subsystem.

## FAT32

FAT32 support is part of the filesystem/disk path and is used by the build/QEMU disk image setup.

The kernel contains disk and disk-I/O layers plus an AHCI driver for accessing block storage.

## Initramfs

The boot initramfs is built as a CPIO `newc` archive and then compressed with LZ4 by the top-level build system.

At boot the kernel:

1. obtains the Limine module;
2. checks for LZ4 frame magic;
3. decompresses it when necessary;
4. parses the CPIO archive;
5. exposes its contents through the VFS;
6. finds `/bin/init`.

## Mounting

The syscall layer contains `mount` support and the VFS has mountpoint-aware directory reading.

The mount syscall is still part of the experimental filesystem interface.

## Current limitations

The VFS is functional enough to support initramfs, devices, userspace file operations and the current FAT32 path, but it is not a Linux-compatible VFS ABI. 
Filesystem coverage and semantics are still evolving.
