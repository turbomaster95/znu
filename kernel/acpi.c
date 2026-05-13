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
#include <pi.h>
#include <timekeeper.h>
#include <uacpi/sleep.h>
#include <sync.h>

extern struct limine_rsdp_request *rsdp_request;
extern void hcf(void);
extern uint64_t rsdp_addr;
extern uint32_t lapic_ticks_per_ms;
static _Atomic uint64_t sub_tick_inc = 0;

static uint32_t pci_get_addr(uacpi_handle device, uacpi_size offset) {
    uint64_t addr = (uint64_t)device;
    uint8_t bus = (addr >> 32) & 0xFF;
    uint8_t dev = (addr >> 16) & 0xFF;
    uint8_t func = addr & 0xFF;
    return (uint32_t)((1U << 31) | (bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC));
}

void uacpi_kernel_vlog(uacpi_log_level lvl, const char *fmt, va_list args) {
    switch (lvl) {
        case UACPI_LOG_INFO:  debugln(fmt, args); break;
        case UACPI_LOG_WARN:  debugwarn(fmt, args); break;
        case UACPI_LOG_ERROR: debugerr(fmt, args);  break;
        default:              vdebugprintf(fmt, args); break;
    }
}

void *uacpi_kernel_alloc(uacpi_size size) {
    void *p = kmalloc(size);
//    debugln("[uACPI] Allocated  %d bytes at %p", (uint32_t)size, p);
    return p;
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size) {
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
//    debugln("[VMM] uACPI requesting map: Phys %p (len %d)", (void*)physical, length);
    (void)length;

    return (void*)(physical + 0xffff800000000000);
}

void uacpi_kernel_unmap(void *ptr, uacpi_size length) {
    (void)ptr;
    (void)length;
}

uacpi_status uacpi_kernel_raw_memory_read(uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 *out_value) {
//    debugln("acpi raw mem read addr: %s", address);
//    debugln("acpi raw mem read byte_width: %s", byte_width);
    void* virt = (void*)((uintptr_t)address + 0xffff800000000000);
    
    if (byte_width == 1) *out_value = *(volatile uint8_t*)virt;
    else if (byte_width == 2) *out_value = *(volatile uint16_t*)virt;
    else if (byte_width == 4) *out_value = *(volatile uint32_t*)virt;
    else if (byte_width == 8) *out_value = *(volatile uint64_t*)virt;
    
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_memory_write(uacpi_phys_addr address, uacpi_u8 byte_width, uacpi_u64 value) {
//    debugln("acpi raw mem write addr: %s", address);
//    debugln("acpi raw mem write byte_width: %s", byte_width);
    uintptr_t virt = address + hhdm_offset;
    if (byte_width == 1) *(volatile uint8_t*)virt = (uint8_t)value;
    else if (byte_width == 2) *(volatile uint16_t*)virt = (uint16_t)value;
    else if (byte_width == 4) *(volatile uint32_t*)virt = (uint32_t)value;
    else if (byte_width == 8) *(volatile uint64_t*)virt = (uint64_t)value;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out) {
	*out = inb((uacpi_io_addr)handle + offset);
	for(int i = 0; i < 10; i++) outb(0x80, 0);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out) {
	*out = inw((uacpi_io_addr)handle + offset);
	for(int i = 0; i < 10; i++) outb(0x80, 0);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out) {
	*out = inl((uacpi_io_addr)handle + offset);
	for(int i = 0; i < 10; i++) outb(0x80, 0);
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
	outl((uacpi_io_addr)handle + offset, v);
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
        case 4: *out = inl(addr); break;
        default: return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_raw_io_write(uacpi_io_addr addr, uacpi_u8 width, uacpi_u64 val) {
    switch (width) {
        case 1: outb(addr, val); break;
        case 2: outw(addr, val); break;
        case 4: outl(addr, val); break;
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

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    uint64_t extra = __atomic_add_fetch(&sub_tick_inc, 100, __ATOMIC_RELAXED);
    //debugln("getnanoseconds called");
    //debugln("nanosnds + extra = %i", (uacpi_u64)timekeeper_timefromboot() + extra);
    return (uacpi_u64)timekeeper_timefromboot() + extra;
}


uacpi_handle uacpi_kernel_create_mutex(void) {
    mutex_t *mutex = kmalloc(sizeof(mutex_t));
    if (!mutex)
        return UACPI_NULL;
    
    mutex_init(mutex);
    return (uacpi_handle)mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    if (!handle)
        return;
    
    mutex_t *mutex = (mutex_t *)handle;
    kfree(mutex);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout) {
    if (!handle)
        return UACPI_STATUS_INVALID_ARGUMENT;
    
    mutex_t *mutex = (mutex_t *)handle;
    
    if (timeout == 0xFFFF) {
        /* Infinite wait */
        mutex_lock(mutex);
        return UACPI_STATUS_OK;
    } else if (timeout == 0) {
        /* Try once, don't block */
        if (mutex_trylock(mutex) != 0)
            return UACPI_STATUS_TIMEOUT;
        return UACPI_STATUS_OK;
    } else {
        /* Timed wait */
        if (mutex_lock_timeout(mutex, timeout) != 0)
            return UACPI_STATUS_TIMEOUT;
        return UACPI_STATUS_OK;
    }
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    if (!handle)
        return;
    
    mutex_t *mutex = (mutex_t *)handle;
    mutex_unlock(mutex);
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    spinlock_t *lock = kmalloc(sizeof(spinlock_t));
    if (!lock)
        return UACPI_NULL;
    
    spinlock_init(lock);
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    if (!handle)
        return;
    
    kfree((spinlock_t *)handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    if (!handle) {
        uacpi_cpu_flags flags;
        __asm__ volatile(
            "pushfq\n"
            "pop %0\n"
            "cli"
            : "=rm"(flags)
            :
            : "memory"
        );
        return flags;
    }
    
    spinlock_t *lock = (spinlock_t *)handle;
    return (uacpi_cpu_flags)spinlock_irq_save(lock);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    if (!handle) {
        __asm__ volatile(
            "push %0\n"
            "popfq"
            :
            : "rm"(flags)
            : "memory", "cc"
        );
        return;
    }
    
    spinlock_t *lock = (spinlock_t *)handle;
    spinlock_irq_restore(lock, (uint64_t)flags);
}

typedef struct {
    semaphore_t sem;
    volatile bool signaled;
} uacpi_event_t;

uacpi_handle uacpi_kernel_create_event(void) {
    uacpi_event_t *event = kmalloc(sizeof(uacpi_event_t));
    if (!event)
        return UACPI_NULL;
    
    semaphore_init(&event->sem, 0);
    event->signaled = false;
    return (uacpi_handle)event;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    if (!handle)
        return;
    
    kfree((uacpi_event_t *)handle);
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
    if (!handle)
        return;
    
    uacpi_event_t *event = (uacpi_event_t *)handle;
    event->signaled = true;
    semaphore_signal(&event->sem);
}


void uacpi_kernel_reset_event(uacpi_handle handle) {
    if (!handle)
        return;
    
    uacpi_event_t *event = (uacpi_event_t *)handle;
    event->signaled = false;
    /* Drain the semaphore back to 0 */
    while (semaphore_try_wait(&event->sem)) {
        /* empty */
    }
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, 
    uacpi_interrupt_handler handler, 
    uacpi_handle ctx, 
    uacpi_handle *out_irq_handle
) {
    (void)irq; (void)handler; (void)ctx;
    *out_irq_handle = (uacpi_handle)1;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, 
    uacpi_handle irq_handle
) {
    (void)handler; (void)irq_handle;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx) {
    handler(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) { return UACPI_STATUS_OK; }

uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (uacpi_thread_id)1; }

void uacpi_kernel_stall(uacpi_u8 usec) {
    debugln("stall called");
    for (uint32_t i = 0; i < (uint32_t)usec; i++) {
        outb(0x80, 0);
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    debugln("sleep called");
    sleep(msec);
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp) {
    uint64_t physical_address = rsdp_addr - hhdm_offset;

    *out_rsdp = (uacpi_phys_addr)physical_address;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req) { return UACPI_STATUS_UNIMPLEMENTED; }

void uacpi_kernel_log(uacpi_log_level lvl, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vdebugprintf(fmt, args);
    va_end(args);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle event, uacpi_u16 timeout) {
    (void)event; 
    (void)timeout; 
    return true; 
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    uacpi_interrupt_state flags;
    __asm__ volatile("pushf; pop %0; cli" : "=rm"(flags) : : "memory");
    return flags;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state flags) {
    __asm__ volatile("push %0; popf" : : "rm"(flags) : "memory", "cc");
}

uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl) {
    (void)current_init_lvl;
    return UACPI_STATUS_OK; 
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 val) {
    outl(0xCF8, pci_get_addr(device, offset));
    outl(0xCFC, val);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 val) {
    outl(0xCF8, pci_get_addr(device, offset));
    outw(0xCFC + (offset & 2), val);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 val) {
    outl(0xCF8, pci_get_addr(device, offset));
    outb(0xCFC + (offset & 3), val);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_deinitialize(void) {
}

// Ensure these names match the undefined references exactly
uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *out) {
    outl(0xCF8, pci_get_addr(device, offset));
    *out = inb(0xCFC + (offset & 3));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *out) {
    outl(0xCF8, pci_get_addr(device, offset));
    *out = inw(0xCFC + (offset & 2));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *out) {
    outl(0xCF8, pci_get_addr(device, offset));
    *out = inl(0xCFC);
    return UACPI_STATUS_OK;
}

void kernel_shutdown(void) {
    debugln("Shutting down...");

    uacpi_status status = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (status != UACPI_STATUS_OK) {
        debugerr("Failed to prepare for sleep: %s", uacpi_status_to_string(status));
    }

    __asm__ volatile("cli");

    status = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);

    debugerr("Shutdown failed: %s", uacpi_status_to_string(status));
    
    outw(0x604, 0x2000);

    hcf();
}

void kernel_reboot(void) {
    debugln("Attempting System Reboot...");

    uacpi_status status = uacpi_reboot();

    if (status != UACPI_STATUS_OK) {
        debugerr("uACPI reboot failed: %s", uacpi_status_to_string(status));
    }

    debugln("Falling back to PS/2 controller reset...");

    outb(0x64, 0xFE);

    debugln("Falling back to Triple Fault...");
    uint16_t idt_limit = 0;
    __asm__ volatile("lidt (%0)" : : "r"(&idt_limit));
    __asm__ volatile("int $3");

    // Ultimate Halt
    hcf();
}

