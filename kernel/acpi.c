// acpi.c - uACPI glue layer for x86_64 Higher Half Direct Map kernel
#include <uacpi/kernel_api.h>
#include <uacpi/uacpi.h>
#include <uacpi/types.h>
#include <uacpi/status.h>
#include <uacpi/namespace.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <string.h>
#include <page.h>
#include <stdlib.h>
#include <stdio.h>
#include <lapic.h>

extern struct limine_rsdp_request *rsdp_request;

void uacpi_kernel_vlog(uacpi_log_level lvl, const char *fmt, va_list args) {
    switch (lvl) {
        case UACPI_LOG_INFO:  debugln(fmt, args); break;
        case UACPI_LOG_WARN:  debugwarn(fmt, args); break;
        case UACPI_LOG_ERROR: debugerr(fmt, args);  break;
        default:              vdebugprintf(fmt, args); break;
    }
}

void *uacpi_kernel_alloc(uacpi_size size) {
	return kmalloc(size);
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size) {
    // Your kmalloc doesn't zero memory by default, 
    // so we must do it here.
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void uacpi_kernel_free(void *ptr) {
	if (ptr == NULL)
		return;

	return kfree(ptr);
}

void *uacpi_kernel_map(uacpi_phys_addr physical, uacpi_size length) {
    uintptr_t phys_start = (uintptr_t)physical & ~0xFFF; // Your PAGE_SIZE is 4096
    uintptr_t offset = (uintptr_t)physical & 0xFFF;
    uintptr_t size_to_map = PAGE_ALIGN_UP(length + offset);
    size_t pages = size_to_map / PAGE_SIZE;

    // Exchange 'vmm_map' for your 'heap_extend' or a virtual range allocator
    // This gets you a fresh virtual address range
    void *virt_base = heap_extend(pages);

    if (virt_base == NULL) {
        return NULL; // uACPI handles NULL as an error
    }

    // Map the pages manually using your 'map_page' function
    uintptr_t curr_v = (uintptr_t)virt_base;
    uintptr_t curr_p = phys_start;

    for (size_t i = 0; i < pages; i++) {
        // Exchange 'ARCH_MMU_FLAGS' for your 'PTE_WRITABLE' etc.
        map_page(vmm_get_kernel_pml4(), curr_v, curr_p, PTE_WRITABLE);
        curr_v += PAGE_SIZE;
        curr_p += PAGE_SIZE;
    }

    return (void *)((uintptr_t)virt_base + offset);
}

void uacpi_kernel_unmap(void *ptr, uacpi_size length) {
	uintmax_t pageoffset = (uintptr_t)ptr % PAGE_SIZE;

	uintptr_t addr = (uintptr_t)ptr;
	unmap_page((void*)ROUND_DOWN(addr, PAGE_SIZE), ROUND_UP(length + pageoffset, PAGE_SIZE));
}

uacpi_status uacpi_kernel_raw_memory_read(uacpi_phys_addr address, uacpi_u8 width, uacpi_u64 *out) {
	void *ptr = uacpi_kernel_map(address, width);

	switch (width) {
		case 1:
			*out = *(volatile uint8_t *)ptr;
			break;
		case 2:
			*out = *(volatile uint16_t *)ptr;
			break;
		case 4:
			*out = *(volatile uint32_t *)ptr;
			break;
		case 8:
			*out = *(volatile uint64_t *)ptr;
			break;
		default:
			uacpi_kernel_unmap(ptr, width);
			return UACPI_STATUS_INVALID_ARGUMENT;
	}

	uacpi_kernel_unmap(ptr, width);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_write(uacpi_phys_addr address, uacpi_u8 width, uacpi_u64 in) {
	void *ptr = uacpi_kernel_map(address, width);

	switch (width) {
		case 1:
			*(volatile uint8_t *)ptr = in;
			break;
		case 2:
			*(volatile uint16_t *)ptr = in;
			break;
		case 4:
			*(volatile uint32_t *)ptr = in;
			break;
		case 8:
			*(volatile uint64_t *)ptr = in;
			break;
		default:
			uacpi_kernel_unmap(ptr, width);
			return UACPI_STATUS_INVALID_ARGUMENT;
	}

	uacpi_kernel_unmap(ptr, width);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out) {
	*out = inb((uacpi_io_addr)handle + offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out) {
	*out = inw((uacpi_io_addr)handle + offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out) {
	*out = ind((uacpi_io_addr)handle + offset);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 v) {
	outb((uacpi_io_addr)handle + offset, v);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 v) {
	outw((uacpi_io_addr)handle + offset, v);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 v) {
	outd((uacpi_io_addr)handle + offset, v);
	return UACPI_STATUS_OK;
}

int __popcountdi2(int64_t a) {
	uint64_t x2 = (uint64_t)a;
	x2 = x2 - ((x2 >> 1) & 0x5555555555555555uLL);
	// Every 2 bits holds the sum of every pair of bits (32)
	x2 = ((x2 >> 2) & 0x3333333333333333uLL) + (x2 & 0x3333333333333333uLL);
	// Every 4 bits holds the sum of every 4-set of bits (3 significant bits) (16)
	x2 = (x2 + (x2 >> 4)) & 0x0F0F0F0F0F0F0F0FuLL;
	// Every 8 bits holds the sum of every 8-set of bits (4 significant bits) (8)
	uint32_t x = (uint32_t)(x2 + (x2 >> 32));
	// The lower 32 bits hold four 16 bit sums (5 significant bits).
	//   Upper 32 bits are garbage
	x = x + (x >> 16);
	// The lower 16 bits hold two 32 bit sums (6 significant bits).
	//   Upper 16 bits are garbage
	return (x + (x >> 8)) & 0x0000007F; // (7 significant bits)
}

uacpi_status uacpi_kernel_raw_io_read(uacpi_io_addr addr, uacpi_u8 width, uacpi_u64 *out) {
    switch (width) {
        case 1: *out = inb(addr); break;
        case 2: *out = inw(addr); break;
        case 4: *out = ind(addr); break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_write(uacpi_io_addr addr, uacpi_u8 width, uacpi_u64 val) {
    switch (width) {
        case 1: outb(addr, val); break;
        case 2: outw(addr, val); break;
        case 4: outd(addr, val); break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size size, uacpi_handle *outhandle) {
	*outhandle = (uacpi_handle)base;
	return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
	(void)handle;
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
	uint64_t v = ((uint64_t)address.segment << 48) | ((uint64_t)address.bus << 32) | ((uint64_t)address.device << 16) | ((uint64_t)address.function);
	*out_handle = (uacpi_handle)v;
	return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
	(void)handle;
}

