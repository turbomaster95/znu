// acpi.c - uACPI glue layer for x86_64 Higher Half Direct Map kernel
#include <uacpi/kernel_api.h>
#include <uacpi/uacpi.h>
#include <uacpi/types.h>
#include <uacpi/status.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <page.h>
#include <stdlib.h> // Contains debugln, kmalloc, kfree
#include <lapic.h>  // Contains lapic_sleep

// Limine RSDP response pointer (Must be populated in kmain before calling init_acpi)
extern struct limine_rsdp_response *rsdp_response;

// ------------------------------------------------------------------
// CPU I/O Helpers (Implementing the widths you don't have yet)
// ------------------------------------------------------------------

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

// ------------------------------------------------------------------
// uACPI OAL Implementation
// ------------------------------------------------------------------

void* uacpi_kernel_alloc(uacpi_size size) {
    debugln("uacpi_kernel_alloc: %zu", (size_t)size);
    return kmalloc(size);
}

void uacpi_kernel_free(void* ptr) {
    kfree(ptr);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    debugln("uacpi_kernel_map: addr: %llx, len: %zu", (uint64_t)addr, (size_t)len);
    return PHYS_TO_VIRT(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {
    (void)addr; (void)len;
}

uacpi_status init_acpi(void) {
    debugln("Initializing ACPI...");
    
    // Check if rsdp_response is actually valid
    if (!rsdp_response || rsdp_response->address == 0) {
        debugln("Error: RSDP response is NULL");
        return UACPI_STATUS_NOT_FOUND;
    }

    // On some versions of uACPI, the RSDP is passed via a params struct,
    // but your header said it takes a u64 flags. 
    // If it takes flags, it EXPECTS uacpi_kernel_get_rsdp to work perfectly.
    
    uacpi_status ret = uacpi_initialize(0); 
    debugln("Got status: %u", (uint32_t)ret);
    
    if (uacpi_likely_error(ret)) return ret;
    return uacpi_namespace_load();
}


void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* str) {
    (void)level;
    debugln("[uACPI] %s", str);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return 0; // Stub: Update with timer/TSC later
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp) {
    if (!rsdp_response || rsdp_response->address == 0) 
        return UACPI_STATUS_NOT_FOUND;
        
    *out_rsdp = (uacpi_phys_addr)rsdp_response->address;
    return UACPI_STATUS_OK;
}


uacpi_cpu_flags uacpi_kernel_disable_interrupts(void) {
    uacpi_cpu_flags flags;
    // pushfq: push RFLAGS to stack
    // pop: load them into the variable
    // cli: clear interrupt flag
    __asm__ volatile(
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=rm"(flags)
        :
        : "memory"
    );
    return flags;
}

void uacpi_kernel_restore_interrupts(uacpi_cpu_flags flags) {
    // Check if the 9th bit (Interrupt Flag) was set in the saved flags
    if (flags & (1 << 9)) {
        __asm__ volatile("sti" : : : "memory");
    }
}

// --- Sync & Threading ---
uacpi_handle uacpi_kernel_create_mutex(void) { return (uacpi_handle)1; }
void uacpi_kernel_free_mutex(uacpi_handle h) { (void)h; }
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle h, uacpi_u16 timeout) { return UACPI_STATUS_OK; }
void uacpi_kernel_release_mutex(uacpi_handle h) { (void)h; }

uacpi_handle uacpi_kernel_create_event(void) { return (uacpi_handle)1; }
void uacpi_kernel_free_event(uacpi_handle h) { (void)h; }
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle h, uacpi_u16 timeout) { return UACPI_TRUE; }
void uacpi_kernel_signal_event(uacpi_handle h) { (void)h; }
void uacpi_kernel_reset_event(uacpi_handle h) { (void)h; }

uacpi_handle uacpi_kernel_create_spinlock(void) { return (uacpi_handle)1; }
void uacpi_kernel_free_spinlock(uacpi_handle h) { (void)h; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) { (void)h; return 0; }
void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags f) { (void)h; (void)f; }

uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (uacpi_thread_id)1; }

// --- Time ---
void uacpi_kernel_sleep(uacpi_u64 msec) {
    sleep(msec);
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    // High-precision micro-stall
    for(volatile int i = 0; i < usec * 1000; i++) __asm__("pause");
}

// --- Interrupts & Work ---
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle *out_irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx) {
    return UACPI_STATUS_UNIMPLEMENTED;
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) { return UACPI_STATUS_OK; }

// --- I/O Mapping ---
uacpi_status uacpi_kernel_io_map(uacpi_phys_addr base, uacpi_size len, uacpi_handle *out_handle) {
    *out_handle = (uacpi_handle)(uintptr_t)base;
    return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle handle) { (void)handle; }

// --- I/O Access (Required Widths) ---
uacpi_status uacpi_kernel_io_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *out) {
    *out = inb((uint16_t)((uintptr_t)device + offset));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *out) {
    *out = inw((uint16_t)((uintptr_t)device + offset));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *out) {
    *out = inl((uint16_t)((uintptr_t)device + offset));
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 val) {
    outb((uint16_t)((uintptr_t)device + offset), val);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 val) {
    outw((uint16_t)((uintptr_t)device + offset), val);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 val) {
    outl((uint16_t)((uintptr_t)device + offset), val);
    return UACPI_STATUS_OK;
}

// --- PCI Stubs ---
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) { return UACPI_STATUS_UNIMPLEMENTED; }
void uacpi_kernel_pci_device_close(uacpi_handle handle) { (void)handle; }
uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value) { return UACPI_STATUS_UNIMPLEMENTED; }
uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value) { return UACPI_STATUS_UNIMPLEMENTED; }
uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value) { return UACPI_STATUS_UNIMPLEMENTED; }
uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) { return UACPI_STATUS_UNIMPLEMENTED; }
uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) { return UACPI_STATUS_UNIMPLEMENTED; }
uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) { return UACPI_STATUS_UNIMPLEMENTED; }

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req) { return UACPI_STATUS_UNIMPLEMENTED; }

