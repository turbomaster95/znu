#include <devkit/core.hpp>

extern "C" {
  #include <page.h>
  #include <pci.h>
  #include <proc.h>
  #include <tendrilg.h>
  #include <tendril/glue.h>
  #include <tendril/tendril.h>
  #include <tendril/hid.h>
  #include <input.h>
  #include <kernel/initcall.h>
}

namespace usbhid {

constexpr size_t kMaxUSBControllers = 16;

znu_tendril_ctx_t gUSBContexts[kMaxUSBControllers];
struct tendril_hc *gUSBControllers[kMaxUSBControllers];
size_t gUSBControllerCount = 0;

class USBHIDDriver : public DKUSBDriver {
public:
    USBHIDDriver() {
        name = "USB-HID Driver";
    }

    int probe(DKService *prov) override {
        auto *usbDev = dynamic_cast<DKUSBDevice *>(prov);
        if (!usbDev) {
            return 0;
        }
        if (usbDev->deviceClass == static_cast<uint8_t>(DKUSBClass::HID) || usbDev->hidKinds != 0) {
            return 100;
        }
        return 0;
    }

    bool start(DKService *prov) override {
        auto *usbDev = dynamic_cast<DKUSBDevice *>(prov);
        if (!usbDev || !attach(usbDev)) {
            return false;
        }
        provider = prov;
        p_info("usb", "USB-HID Driver started for %04x:%04x (kinds=0x%x)",
               usbDev->vendorID, usbDev->productID, usbDev->hidKinds);
        return true;
    }

    void stop(DKService *prov) override {
        auto *usbDev = dynamic_cast<DKUSBDevice *>(prov);
        if (usbDev) {
            p_info("usb", "USB-HID Driver stopped for %04x:%04x", usbDev->vendorID, usbDev->productID);
        }
        detach();
    }

    void handleKey(uint16_t key, bool pressed, uint8_t modifiers) {
#ifdef KRNLDBG
        p_trace("usb", "HID key: 0x%02x pressed=%d modifiers=0x%02x", key, pressed, modifiers);
#endif
        if (device) {
            auto *usbDev = static_cast<DKUSBDevice *>(device);
            if (usbDev->hidKinds && !(usbDev->hidKinds & TENDRIL_HID_KEYBOARD)) {
                return;
            }
        }
        keyboard_handle_hid_key(key, pressed, modifiers);
    }
};

USBHIDDriver *gActiveHIDDriver = nullptr;

DKService *createUSBHIDDriver() {
    return new USBHIDDriver();
}

DKDriverFactory gUSBHIDFactory = {
    "usb-hid",
    createUSBHIDDriver,
    nullptr
};

void onHIDAttach(struct tendril_hid *hid) {
    p_info("usb", "HID device attached (kinds=0x%x, boot=%d)",
           tendril_hid_kinds(hid), tendril_hid_boot_protocol(hid));
}

void onHIDDetach(struct tendril_hid *hid) {
    (void)hid;
    p_info("usb", "HID device detached");
}

void onHIDKey(struct tendril_hid *hid, uint16_t key, bool pressed, uint8_t modifiers) {
    if (!(tendril_hid_kinds(hid) & TENDRIL_HID_KEYBOARD)) {
        return;
    }
    if (gActiveHIDDriver) {
        gActiveHIDDriver->handleKey(key, pressed, modifiers);
    } else {
#ifdef KRNLDBG
        p_trace("usb", "HID key: 0x%02x pressed=%d modifiers=0x%02x", key, pressed, modifiers);
#endif
        keyboard_handle_hid_key(key, pressed, modifiers);
    }
}

const struct tendril_hid_callbacks gHIDCallbacks = {
    .attach = onHIDAttach,
    .detach = onHIDDetach,
    .key = onHIDKey
};

void pollThreadEntry(void *arg) {
    (void)arg;
    p_info("usb", "USB polling thread started");

    while (true) {
        for (size_t i = 0; i < gUSBControllerCount; i++) {
            if (gUSBControllers[i]) {
                tendril_hc_poll(gUSBControllers[i]);
            }
        }
        kthread_sleep_ms(1);
    }
}

} // namespace usbhid

using namespace usbhid;

extern "C" void usb_init(void) {
    tendril_hid_register(&gHIDCallbacks, TENDRIL_HID_BOOT_PROTOCOL);

    for (int i = 0; i < pci_device_count && gUSBControllerCount < kMaxUSBControllers; i++) {
        pci_device_t *dev = &pci_devices[i];
        if (!dev->present || dev->class_code != 0x0C || dev->subclass != 0x03) {
            continue;
        }

        int type = 0;
        uint32_t class_code = (static_cast<uint32_t>(dev->class_code) << 16) |
                              (static_cast<uint32_t>(dev->subclass) << 8) |
                              dev->prog_if;

        if (!tendril_pci_hc_type(class_code, &type)) {
            continue;
        }

        volatile void *mmio = nullptr;
        if (dev->bar_type[0] == PCI_BAR_MMIO && dev->bar_base[0]) {
            mmio = PHYS_TO_VIRT(dev->bar_base[0]);
        }

        znu_tendril_ctx_t *ctx = &gUSBContexts[gUSBControllerCount];
        znu_tendril_init(ctx, dev, mmio);

        struct tendril_hc_config config = {
            .type = type,
            .ctx = ctx,
            .pci = true,
            .irq = false,
            .quirks = tendril_pci_hc_quirks(type, dev->vendor_id, dev->device_id)
        };

        struct tendril_hc *hc = nullptr;
        int result = tendril_hc_add(&config, &hc);
        if (result != 0 || !hc) {
            p_error("usb", "Failed to initialize USB controller %04x:%04x (error %d)",
                    dev->vendor_id, dev->device_id, result);
            continue;
        }

        gUSBControllers[gUSBControllerCount++] = hc;
        p_info("usb", "Initialized USB controller %04x:%04x", dev->vendor_id, dev->device_id);
    }
}

extern "C" void usb_poll(void) {
    for (size_t i = 0; i < gUSBControllerCount; i++) {
        if (gUSBControllers[i]) {
            tendril_hc_poll(gUSBControllers[i]);
        }
    }
}

extern "C" int usb_device_init(void) {
    usb_init();

    uint64_t deadline = tendril_glue_time_ns() + 2000000000ULL;
    while (tendril_glue_time_ns() < deadline) {
        usb_poll();

        bool ready = true;
        for (size_t i = 0; i < gUSBControllerCount; i++) {
            if (!tendril_hc_ready(gUSBControllers[i])) {
                ready = false;
            }
        }
        if (ready) {
            break;
        }
    }

    if (!kthread_create(pollThreadEntry, nullptr, "usb-poll")) {
        p_error("usb", "Failed to create USB polling thread");
        return -1;
    }

    return 0;
}

device_initcall(usb_device_init);

extern "C" int usb_dump(void) {
    p_info("usb", "USB controller count: %zu", gUSBControllerCount);

    for (size_t i = 0; i < gUSBControllerCount; i++) {
        struct tendril_hc *hc = gUSBControllers[i];
        p_info("usb", "Controller %zu: ports=%zu settled=%d ready=%d",
               i, tendril_hc_num_ports(hc), tendril_hc_settled(hc), tendril_hc_ready(hc));

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
