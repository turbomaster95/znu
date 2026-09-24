# Memory Management

Znu's memory-management code is under `mm/`, with architecture-facing page definitions in `include/page.h`.

## Physical memory

`mm/pmm.c` consumes the Limine memory map and selects usable memory for the physical allocator.

The physical allocator uses the buddy allocator from `include/buddy.h`.

Implemented interfaces include:

- `palloc()`
- `palloc_contig()`
- `palloc_zero()`
- `pfree()`

The allocator maintains physical addresses while using the HHDM to access the backing memory.

## HHDM

The kernel defines:

```c
PHYS_TO_VIRT(addr)
VIRT_TO_PHYS(addr)
```

using the Limine-provided HHDM offset.

This is used throughout the PMM/VMM code to access physical memory from the kernel's higher-half address space.

## Virtual memory

`mm/vmm.c` implements:

- switching CR3/PML4s
- contiguous region mapping
- virtual-to-physical translation
- kernel PML4 creation
- kernel-half cloning
- user address-space cloning
- user page-table teardown

The implementation supports normal 4 KiB pages and 2 MiB huge pages. The translation code also recognizes 1 GiB pages.

The kernel address space retains the higher-half mappings and creates HHDM mappings for the physical memory described by the boot memory map.

## Page mapping

`mm/mappage.c` and `mm/unmappage.c` provide page-table operations.

`include/page.h` defines the PTE flags, page-size constants and address-conversion helpers.

## Kernel allocation

The memory subsystem exposes kernel allocation interfaces such as:

- `kmalloc`
- `kfree`
- `kzalloc`
- `krealloc`

A slab allocator is implemented in `mm/slab.c`.

## User address spaces

Each process has a PML4 pointer.

When a process is created or cloned, the user portion of the address space can be copied while the kernel half remains shared.

The current clone implementation performs a deep copy of mapped 4 KiB user pages. Huge user pages are explicitly skipped by the cloning path.

## Current limitations

The source is an experimental implementation rather than a production VM subsystem. In particular:

- user huge-page cloning is not implemented;
- PMM free-page accounting is currently a simple arena-size calculation rather than a full live buddy walk;
- address-space management is closely coupled to the current process/ELF implementation.
