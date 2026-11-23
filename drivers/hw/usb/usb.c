#include <page.h>
#include <pci.h>
#include <proc.h>
#include <tendrilg.h>
#include <tendril/glue.h>
#include <tendril/tendril.h>
#include <tendril/hid.h>
#include <input.h>
#include <kernel/initcall.h>

#define ZNU_MAX_USB_CONTROLLERS 16

static znu_tendril_ctx_t usb_contexts[ZNU_MAX_USB_CONTROLLERS];
static struct tendril_hc *usb_controllers[ZNU_MAX_USB_CONTROLLERS];
static size_t usb_controller_count = 0;

static void usb_hid_attach(struct tendril_hid *hid) {
    p_info("usb", "HID device attached (kinds=0x%x, boot=%d)", tendril_hid_kinds(hid), tendril_hid_boot_protocol(hid));
}

static void usb_hid_detach(struct tendril_hid *hid) {
    p_info("usb", "HID device detached");
}

static void usb_hid_key(struct tendril_hid *hid, uint16_t key, bool pressed, uint8_t modifiers) {
#ifdef KRNLDBG
    p_trace("usb", "HID key: 0x%02x pressed=%d modifiers=0x%02x", key, pressed, modifiers);
#endif
    if (!(tendril_hid_kinds(hid) & TENDRIL_HID_KEYBOARD)) return;
    keyboard_handle_hid_key(key, pressed, modifiers);
}

static const struct tendril_hid_callbacks usb_hid_callbacks = {
    .attach = usb_hid_attach,
    .detach = usb_hid_detach,
    .key = usb_hid_key
};

void usb_init(void) {
    tendril_hid_register(&usb_hid_callbacks, TENDRIL_HID_BOOT_PROTOCOL);
    for (int i = 0; i < pci_device_count && usb_controller_count < ZNU_MAX_USB_CONTROLLERS; i++) {
        pci_device_t *dev = &pci_devices[i];
        if (!dev->present || dev->class_code != 0x0C || dev->subclass != 0x03) continue;

        int type = 0;
        uint32_t class_code = ((uint32_t)dev->class_code << 16) | ((uint32_t)dev->subclass << 8) | dev->prog_if;

        if (!tendril_pci_hc_type(class_code, &type)) continue;

        volatile void *mmio = NULL;

        if (dev->bar_type[0] == PCI_BAR_MMIO && dev->bar_base[0]) {
            mmio = PHYS_TO_VIRT(dev->bar_base[0]);
        }

        znu_tendril_ctx_t *ctx = &usb_contexts[usb_controller_count];
        znu_tendril_init(ctx, dev, mmio);

        struct tendril_hc_config config = {
            .type = type,
            .ctx = ctx,
            .pci = true,
            .irq = false,
            .quirks = tendril_pci_hc_quirks(type, dev->vendor_id, dev->device_id)
        };

        struct tendril_hc *hc = NULL;
        int result = tendril_hc_add(&config, &hc);

        if (result != 0 || !hc) {
            p_error("usb", "Failed to initialize USB controller %04x:%04x (error %d)", dev->vendor_id, dev->device_id, result);
            continue;
        }

        usb_controllers[usb_controller_count++] = hc;
        p_info("usb", "Initialized USB controller %04x:%04x", dev->vendor_id, dev->device_id);
    }
}

void usb_poll(void) {
    for (size_t i = 0; i < usb_controller_count; i++) {
        if (usb_controllers[i]) tendril_hc_poll(usb_controllers[i]);
    }
}

static void usb_poll_thread(void *arg) {
    (void)arg;
    p_info("usb", "USB polling thread started");

    while (1) {
        usb_poll();
        kthread_sleep_ms(1);
    }
}

static int usb_device_init(void) {
    usb_init();

    uint64_t deadline = tendril_glue_time_ns() + 2000000000ULL;

    while (tendril_glue_time_ns() < deadline) {
        usb_poll();

        bool ready = true;
        for (size_t i = 0; i < usb_controller_count; i++) {
            if (!tendril_hc_ready(usb_controllers[i])) ready = false;
        }

        if (ready) break;
    }

    if (!kthread_create(usb_poll_thread, NULL, "usb-poll")) {
        p_error("usb", "Failed to create USB polling thread");
        return -1;
    }

    return 0;
}

device_initcall(usb_device_init);

int usb_dump(void) {
    p_info("usb", "USB controller count: %zu", usb_controller_count);

    for (size_t i = 0; i < usb_controller_count; i++) {
        struct tendril_hc *hc = usb_controllers[i];

        p_info("usb", "Controller %zu: ports=%zu settled=%d ready=%d", i, tendril_hc_num_ports(hc), tendril_hc_settled(hc), tendril_hc_ready(hc));

        for (uint8_t port = 1; port <= tendril_hc_num_ports(hc); port++) {
            struct tendril_device *dev = tendril_hc_port_device(hc, port);

            if (dev) {
                p_info("usb", "Device detected on controller %zu port %u", i, port);
            } else {
                p_debug("usb", "No enumerated device on controller %zu port %u", i, port);
            }
        }
    }

    return 0;
}
