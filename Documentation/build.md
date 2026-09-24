# Build System

Znu uses a heavily modified Linux Kernel derived Kbuild build system with a all-in-one bootstrap script.

## Entry point

Use:

```sh
./build.sh <operation>
```

The current operations are:

```text
build
kernel
tools
clean
```

Useful options include:

```text
-j njob
-T tools
-u
```

Additional arguments are forwarded to the generated make command.

## Typical build

Build the host tools first:

```sh
./build.sh tools
```

Build the kernel:

```sh
./build.sh kernel
```

Or perform the combined build:

```sh
./build.sh build
```

The build script constructs an in-tree tool directory and uses Znu-prefixed host tools such as `zngmake`, `znlz4`, `znnasm`, `znxorriso`, and related utilities.

## Un-Typical build

The tools build can be completely skipped by running make manually with a few flags.
This can be finnicky, as some of the host tools might not be the version/type that Znu/Kbuild expects. (e.g BSD awk instead of GNU awk)

To Build the kernel:
```sh
make -f ZNUmakefile INREPO=yes
```

## Configuration

The repository has a top-level Kconfig:

```text
Kconfig
```

Subsystem configuration is spread across:

```text
kernel/Kconfig
drivers/Kconfig
lib/Kconfig
kernel/filters/Kconfig
yuki/Kconfig
```

Configuration output is generated under `include/config` and `include/generated`.

The checked-in default configuration is:

```text
configs/defconfig
```

## ISO generation

When `GENERATE_ISO` is enabled, the build can produce:

```text
Znu.iso
Znu.bios.iso
Znu.uefi.iso
```

depending on:

```text
ISO_MULTI
ISO_BIOS
ISO_UEFI
```

The ISO is assembled with Limine.

## Initramfs

The build system takes the contents of:

```text
configs/sysroot/
```

and creates a CPIO `newc` archive.

That archive is compressed using LZ4 and placed at:

```text
configs/iso_root/boot/initramfs.cpio
```

## QEMU

The top-level build target has a QEMU run configuration using:

- x86_64
- KVM
- the generated BIOS ISO
- an AHCI controller
- a FAT32 disk image
- a QEMU user-mode network backend
- an E1000 NIC

The build system also provides a FAT32 image generation target.

## Source/build split

The important top-level build files are:

```text
Kbuild
Kconfig
ZNUmakefile
build.sh
```

Subsystem Makefiles live inside their corresponding source directories.

The existing `Documentation/kbuild/` files document the Kbuild/Kconfig layer in more detail.

## Cleaning

The build system distinguishes between:

```sh
make clean
make mrproper
make distclean
```

The `build.sh clean` operation invokes the configured make environment when available.

`mrproper` removes generated configuration/build state, while `distclean` additionally removes editor/patch leftovers and other generated debris.

To use the bare `make` commands in this section, please refer to the [Un-Typical Build](#un-typical-build) section.

## Cross compilation

The configuration supports a `CROSS_COMPILE` prefix and an alternate Clang/LLVM build through `CLANG`.

The kernel build is freestanding and uses `-nostdlib` with kernel-specific compiler flags.
