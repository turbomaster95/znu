# Drivers and Networking

## Driver configuration

The top-level driver Kconfig currently exposes:

- `BGA` - Bochs Graphic Adapter
- `E1000` - Intel PRO/1000
- `AHCI` - AHCI storage

Drivers live under `drivers/`, with some platform-facing support under `arch/x86/`.

## PCI

`arch/x86/pci.c` provides PCI enumeration used by hardware drivers.

## AHCI

The AHCI driver is under:

```text
drivers/block/ahci/
```

The kernel disk layer is in `kernel/disk.c` and `kernel/diskio.c`.

The QEMU configuration in the build system attaches an AHCI controller and a FAT32 disk image.

## Bochs/BGA framebuffer

The virtual framebuffer driver is under:

```text
drivers/fb/virt/bochs/
```

It provides the optional BGA/Bochs graphics path in addition to the normal Limine framebuffer path.

## Intel E1000

The E1000 driver is under:

```text
drivers/net/intel/e1000/
```

The source defines RX/TX descriptor rings and exposes operations for:

- initialization
- packet transmit
- packet receive
- MAC retrieval
- link status
- interrupt enable/disable
- interrupt status
- interrupt handling

The current driver is configured around the E1000 devices used by the QEMU setup.

## Network stack

`drivers/net/net.c` implements a small Ethernet/IP stack.

The source currently contains support for:

- Ethernet frames
- ARP
- IPv4
- TCP
- UDP packet structures
- DNS resolution
- TCP connect/send/receive/close

The stack uses an ARP cache and a small fixed TCP connection table.

The test suite includes ARP, DNS, TCP and HTTP test paths. UDP echo is explicitly marked as requiring further receive implementation.

## Network test path

`kernel/tests.c` contains network tests for:

1. ARP resolution
2. DNS resolution
3. TCP connection
4. simple HTTP GET
5. UDP echo test placeholder

The tests are not enabled as a claim of production network compatibility; they are runtime experiments for the current stack.

## Other hardware

The architecture tree also contains:

- PIC
- LAPIC
- PIT
- RTC
- PS/2
- serial
- PCI
- ACPI/uACPI support

The VFS devfs path additionally exposes input and framebuffer device nodes.
