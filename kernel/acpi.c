// acpi.c - uACPI glue layer for x86_64 Higher Half Direct Map kernel
#include <uacpi/uacpi.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

// ------------------------------------------------------------------
// Kernel external symbols and functions (provided by your kernel)
// ------------------------------------------------------------------
extern uint64_t hhdm_offset;
extern uint64_t *kernel_pml4;

// Limine RSDP response pointer
struct limine_rsdp_response *rsdp_response = NULL;


void *kmalloc(size_t size);
void kfree(void *ptr);
void debugln(const char *fmt, ...);
void map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);


// ------------------------------------------------------------------
// x86 I/O port inline assembly
// ------------------------------------------------------------------
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
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
// Spinlock internal representation (opaque to uACPI)
// ------------------------------------------------------------------
typedef volatile uacpi_u32 spinlock_t;

// ------------------------------------------------------------------
// Event internal representation
// ------------------------------------------------------------------
struct uacpi_event {
    volatile bool signaled;
};

// ------------------------------------------------------------------
// Work callback type (may be defined in kernel_api.h, but declare if missing)
// ------------------------------------------------------------------
#ifndef UACPI_WORK_CALLBACK_DEFINED
typedef void (*uacpi_work_callback)(uacpi_handle ctx);
#endif

// ------------------------------------------------------------------
// 1. RSDP retrieval
// ------------------------------------------------------------------
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if (rsdp_response == NULL || rsdp_response->address == 0) {
        debugln("uACPI: ERROR - No RSDP found");
        return UACPI_STATUS_NOT_FOUND;
    }
    *out_rsdp_address = (uacpi_phys_addr)rsdp_response->address;
    debugln("uACPI: RSDP provided by Limine at physical address 0x%lx", *out_rsdp_address);
    return UACPI_STATUS_OK;
}

// ------------------------------------------------------------------
// 2. Memory mapping
// ------------------------------------------------------------------
void *uacpi_kernel_map(uacpi_phys_addr phys, uacpi_size len) {
    void *virt = (void *)(phys + hhdm_offset);
    debugln("uACPI: MAP   phys=0x%lx -> virt=%p (len=0x%zx)", phys, virt, len);
    return virt;
}

void uacpi_kernel_unmap(void *virt, uacpi_size len) {
    debugln("uACPI: UNMAP virt=%p (len=0x%zx)", virt, len);
    (void)virt; (void)len;
}

// ------------------------------------------------------------------
// 3. Heap allocation
// ------------------------------------------------------------------
void *uacpi_kernel_alloc(uacpi_size size) {
    void *ptr = kmalloc(size);
    if (ptr == NULL) {
        debugln("uACPI: ALLOC failed - requested %zu bytes", size);
    } else {
        debugln("uACPI: ALLOC %zu bytes -> %p", size, ptr);
    }
    return ptr;
}

void uacpi_kernel_free(void *ptr) {
    debugln("uACPI: FREE  %p", ptr);
    kfree(ptr);
}

// ------------------------------------------------------------------
// 4. Logging
// ------------------------------------------------------------------
void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char *str) {
    const char *prefix;
    switch (lvl) {
        case UACPI_LOG_DEBUG: prefix = "DEBUG"; break;
        case UACPI_LOG_TRACE: prefix = "TRACE"; break;
        case UACPI_LOG_INFO:  prefix = "INFO "; break;
        case UACPI_LOG_WARN:  prefix = "WARN "; break;
        case UACPI_LOG_ERROR: prefix = "ERROR"; break;
        default:              prefix = "?????"; break;
    }
    debugln("uACPI [%s]: %s", prefix, str);
}

// ------------------------------------------------------------------
// 5. Direct I/O ports
// ------------------------------------------------------------------
uacpi_u8 uacpi_kernel_read_io8(uacpi_io_addr port) {
    return inb(port);
}
void uacpi_kernel_write_io8(uacpi_io_addr port, uacpi_u8 val) {
    outb(port, val);
}
uacpi_u16 uacpi_kernel_read_io16(uacpi_io_addr port) {
    return inw(port);
}
void uacpi_kernel_write_io16(uacpi_io_addr port, uacpi_u16 val) {
    outw(port, val);
}
uacpi_u32 uacpi_kernel_read_io32(uacpi_io_addr port) {
    return inl(port);
}
void uacpi_kernel_write_io32(uacpi_io_addr port, uacpi_u32 val) {
    outl(port, val);
}

// ------------------------------------------------------------------
// 6. Memory‑mapped I/O (handle‑based)
// ------------------------------------------------------------------
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    void *virt = (void *)(base + hhdm_offset);
    *out_handle = virt;
    debugln("uACPI: IOMAP base=0x%lx -> virt=%p (len=0x%zx)", base, virt, len);
    return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle handle) {
    debugln("uACPI: IOUNMAP handle=%p", handle);
    (void)handle;
}
uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size off, uacpi_u8 *out) {
    *out = *(volatile uacpi_u8 *)((uintptr_t)h + off);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size off, uacpi_u8 val) {
    *(volatile uacpi_u8 *)((uintptr_t)h + off) = val;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size off, uacpi_u16 *out) {
    *out = *(volatile uacpi_u16 *)((uintptr_t)h + off);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size off, uacpi_u16 val) {
    *(volatile uacpi_u16 *)((uintptr_t)h + off) = val;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size off, uacpi_u32 *out) {
    *out = *(volatile uacpi_u32 *)((uintptr_t)h + off);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size off, uacpi_u32 val) {
    *(volatile uacpi_u32 *)((uintptr_t)h + off) = val;
    return UACPI_STATUS_OK;
}

// ------------------------------------------------------------------
// 7. Timekeeping
// ------------------------------------------------------------------
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    static uacpi_u64 ns = 0;
    ns += 1000000ULL; // 1 ms per call for demo
    return ns;
}
void uacpi_kernel_stall(uacpi_u8 usec) {
    volatile uacpi_u32 count = usec * 1000;
    for (volatile uacpi_u32 i = 0; i < count; i++) {
        __asm__ volatile("pause");
    }
}
void uacpi_kernel_sleep(uacpi_u64 msec) {
    while (msec > 0) {
        uacpi_u8 chunk = (msec > 255) ? 255 : (uacpi_u8)msec;
        uacpi_kernel_stall(chunk * 1000);
        msec -= chunk;
    }
}

// ------------------------------------------------------------------
// 8. Spinlocks
// ------------------------------------------------------------------
uacpi_handle uacpi_kernel_create_spinlock(void) {
    spinlock_t *lock = kmalloc(sizeof(spinlock_t));
    if (lock) *lock = 0;
    return (uacpi_handle)lock;
}
void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    kfree(lock);
}
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    // Save interrupt state and disable interrupts
    uacpi_cpu_flags flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    spinlock_t *l = (spinlock_t *)lock;
    while (__sync_lock_test_and_set(l, 1)) {
        __asm__ volatile("pause");
    }
    return flags;
}
void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags flags) {
    spinlock_t *l = (spinlock_t *)lock;
    __sync_lock_release(l);
    // Restore interrupt state
    __asm__ volatile("push %0; popfq" : : "r"(flags) : "memory");
}

// ------------------------------------------------------------------
// 9. Events
// ------------------------------------------------------------------
uacpi_handle uacpi_kernel_create_event(void) {
    struct uacpi_event *ev = kmalloc(sizeof(*ev));
    if (ev) ev->signaled = false;
    return ev;
}
void uacpi_kernel_free_event(uacpi_handle ev) {
    kfree(ev);
}
void uacpi_kernel_signal_event(uacpi_handle ev) {
    ((struct uacpi_event *)ev)->signaled = true;
}
void uacpi_kernel_reset_event(uacpi_handle ev) {
    ((struct uacpi_event *)ev)->signaled = false;
}
uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle ev, uacpi_u16 timeout_ms) {
    (void)timeout_ms;
    return UACPI_TRUE;  // For single‑threaded, assume immediate success
}

// ------------------------------------------------------------------
// 10. Work scheduling
// ------------------------------------------------------------------
uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler cb, uacpi_handle ctx) {
    cb(ctx);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}

// ------------------------------------------------------------------
// 11. Mutexes
// ------------------------------------------------------------------
uacpi_handle uacpi_kernel_create_mutex(void) {
    static int dummy = 0;
    return &dummy;
}
void uacpi_kernel_free_mutex(uacpi_handle mutex) {
    (void)mutex;
}
uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle mutex, uacpi_u16 timeout_ms) {
    (void)mutex; (void)timeout_ms;
    return UACPI_STATUS_OK;
}
void uacpi_kernel_release_mutex(uacpi_handle mutex) {
    (void)mutex;
}

// ------------------------------------------------------------------
// 12. Thread ID
// ------------------------------------------------------------------
uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return 0;
}

// ------------------------------------------------------------------
// 13. Interrupt handling
// ------------------------------------------------------------------
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq,
                                                    uacpi_interrupt_handler handler,
                                                    uacpi_handle ctx,
                                                    uacpi_handle *out_handle)
{
    *out_handle = (uacpi_handle)0xDEADBEEF;
    (void)irq; (void)handler; (void)ctx;
    debugln("uACPI: install interrupt handler for IRQ %u (stub)", irq);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler,
                                                      uacpi_handle ctx)
{
    (void)handler; (void)ctx;
    debugln("uACPI: uninstall interrupt handler (stub)");
    return UACPI_STATUS_OK;
}

// ------------------------------------------------------------------
// 14. Interrupt enable/disable (returns previous state)
// ------------------------------------------------------------------
uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    uacpi_interrupt_state flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    __asm__ volatile("push %0; popfq" : : "r"(state) : "memory");
}

// ------------------------------------------------------------------
// 15. PCI device access
// ------------------------------------------------------------------
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address addr, uacpi_handle *out) {
    *out = (uacpi_handle)0xCAFEBABE;
    debugln("uACPI: PCI device open stub (seg=%u, bus=%u, dev=%u, func=%u)",
            addr.segment, addr.bus, addr.device, addr.function);
    return UACPI_STATUS_OK;
}
void uacpi_kernel_pci_device_close(uacpi_handle dev) {
    (void)dev;
}
uacpi_status uacpi_kernel_pci_read8(uacpi_handle dev, uacpi_size off, uacpi_u8 *val) {
    *val = 0xFF; (void)dev; (void)off;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write8(uacpi_handle dev, uacpi_size off, uacpi_u8 val) {
    (void)dev; (void)off; (void)val;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read16(uacpi_handle dev, uacpi_size off, uacpi_u16 *val) {
    *val = 0xFFFF; (void)dev; (void)off;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write16(uacpi_handle dev, uacpi_size off, uacpi_u16 val) {
    (void)dev; (void)off; (void)val;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read32(uacpi_handle dev, uacpi_size off, uacpi_u32 *val) {
    *val = 0xFFFFFFFF; (void)dev; (void)off;
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write32(uacpi_handle dev, uacpi_size off, uacpi_u32 val) {
    (void)dev; (void)off; (void)val;
    return UACPI_STATUS_OK;
}

// ------------------------------------------------------------------
// 16. Firmware request handler
// ------------------------------------------------------------------
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

// ------------------------------------------------------------------
// 17. OSI handling
// ------------------------------------------------------------------
//uacpi_bool uacpi_handle_osi(const uacpi_char *interface) {
  //  debugln("uACPI: OSI query for '%s' -> false", interface);
   // return UACPI_FALSE;
//}

// ------------------------------------------------------------------
// 18. Interface init/deinit
// ------------------------------------------------------------------
//void uacpi_initialize_interfaces(void) {
//}
//void uacpi_deinitialize_interfaces(void) {
//}

// ------------------------------------------------------------------
// 19. ACPI initialization
// ------------------------------------------------------------------
void init_acpi(void) {
    debugln("Initializing ACPI subsystem via uACPI...");
    uacpi_status ret;

    ret = uacpi_initialize(0);
    if (ret != UACPI_STATUS_OK) {
        debugln("uACPI: uacpi_initialize failed with status %d", ret);
        return;
    }
    debugln("uACPI: uacpi_initialize succeeded");

    ret = uacpi_namespace_load();
    if (ret != UACPI_STATUS_OK) {
        debugln("uACPI: uacpi_namespace_load failed with status %d", ret);
        return;
    }
    debugln("uACPI: uacpi_namespace_load succeeded");

    ret = uacpi_namespace_initialize();
    if (ret != UACPI_STATUS_OK) {
        debugln("uACPI: uacpi_namespace_initialize failed with status %d", ret);
        return;
    }
    debugln("uACPI: uacpi_namespace_initialize succeeded – ACPI is ready.");
}
