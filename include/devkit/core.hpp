#pragma once

#include <stdint.h>
#include <stddef.h>

extern "C" {
#include <stdlib.h>
#include <devkit/devkit.h>
#include <tendril/tendril.h>
#include <tendril/hid.h>
#include <tendrilg.h>
}

class DKObject {
    int refCount = 1;
public:
    virtual ~DKObject() = default;
    void retain() { __atomic_add_fetch(&refCount, 1, __ATOMIC_SEQ_CST); }
    void release() {
        if (__atomic_sub_fetch(&refCount, 1, __ATOMIC_SEQ_CST) == 0) {
            delete this;
        }
    }
};

template<typename T>
class DKRef {
    T *ptr = nullptr;

public:
    DKRef() = default;
    DKRef(T *object) : ptr(object) { if (ptr) ptr->retain(); }
    DKRef(const DKRef& other) : ptr(other.ptr) { if (ptr) ptr->retain(); }
    DKRef(DKRef&& other) : ptr(other.ptr) { other.ptr = nullptr; }
    ~DKRef() { if (ptr) ptr->release(); }

    DKRef& operator=(const DKRef& other) {
        if (this != &other) {
            T *next = other.ptr;
            if (next) next->retain();
            if (ptr) ptr->release();
            ptr = next;
        }
        return *this;
    }

    DKRef& operator=(DKRef&& other) {
        if (this != &other) {
            if (ptr) ptr->release();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    static DKRef adopt(T *object) {
        DKRef ref;
        ref.ptr = object;
        return ref;
    }

    void reset() { if (ptr) ptr->release(); ptr = nullptr; }
    T *get() const { return ptr; }
    T *operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

template<typename T>
class DKPtr {
    T *ptr = nullptr;

public:
    DKPtr() = default;
    DKPtr(T *object) : ptr(object) {}

    T *get() const { return ptr; }
    T *operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    explicit operator bool() const { return ptr != nullptr; }
    void reset() { ptr = nullptr; }
};

class DKService : public DKObject {
protected:
    DKService *provider = nullptr;
    DKService *firstChild = nullptr;
    DKService *nextSibling = nullptr;
    const char *name = "DKService";

public:
    virtual bool init() { return true; }
    virtual int probe(DKService *prov) { (void)prov; return 0; }
    virtual bool start(DKService *prov) { provider = prov; return true; }
    virtual void stop(DKService *prov) { (void)prov; }
    virtual bool isUSBHostController() const { return false; }

    void attachChild(DKService *child) {
        if (!child) return;
        child->retain();
        child->provider = this;
        child->nextSibling = firstChild;
        firstChild = child;
    }

    const char *getName() const { return name; }
    DKService *getProvider() const { return provider; }
    DKService *getFirstChild() const { return firstChild; }
    DKService *getNextSibling() const { return nextSibling; }
};

class DKPCIResource {
public:
    enum Type : uint8_t {
        None = DEVKIT_PCI_BAR_NONE,
        MMIO = DEVKIT_PCI_BAR_MMIO,
        IO   = DEVKIT_PCI_BAR_IO
    };

    Type type = None;
    uint64_t base = 0;
    uint64_t size = 0;
    uint8_t width = 0;
    bool prefetchable = false;

    bool isValid() const { return type != None && size != 0; }
};

class DKPCIDevice : public DKService {
    DKPCIResource bars[6];

public:
    uint8_t bus, slot, func;
    uint16_t vendorID, deviceID;
    uint8_t irq;

    DKPCIDevice(uint8_t b, uint8_t s, uint8_t f, uint16_t vid, uint16_t did, const devkit_pci_bar_t resources[6], uint8_t i)
        : bus(b), slot(s), func(f), vendorID(vid), deviceID(did), irq(i) {
        name = "DKPCIDevice";
        for (unsigned int n = 0; n < 6; n++) {
            bars[n].type = static_cast<DKPCIResource::Type>(resources[n].type);
            bars[n].base = resources[n].base;
            bars[n].size = resources[n].size;
            bars[n].width = resources[n].width;
            bars[n].prefetchable = resources[n].prefetchable != 0;
        }
    }

    const DKPCIResource *getBAR(unsigned int index) const { return index < 6 ? &bars[index] : nullptr; }
};

enum class DKUSBSpeed : uint8_t {
    Unknown = 0,
    Low = 1,
    Full = 2,
    High = 3,
    Super = 4,
    SuperPlus = 5
};

enum class DKUSBClass : uint8_t {
    PerInterface = 0x00,
    Audio = 0x01,
    Comm = 0x02,
    HID = 0x03,
    Physical = 0x05,
    Image = 0x06,
    Printer = 0x07,
    MassStorage = 0x08,
    Hub = 0x09,
    CDCData = 0x0A,
    SmartCard = 0x0B,
    ContentSecurity = 0x0D,
    Video = 0x0E,
    PersonalHealthcare = 0x0F,
    AudioVideo = 0x10,
    Billboard = 0x11,
    TypeCBridge = 0x12,
    Diagnostic = 0xDC,
    WirelessController = 0xE0,
    Miscellaneous = 0xEF,
    ApplicationSpecific = 0xFE,
    VendorSpecific = 0xFF
};

class DKUSBHostController : public DKService {
public:
    struct tendril_hc *hc = nullptr;
    struct znu_tendril_ctx *ctx = nullptr;
    int hcType = 0;
    uint32_t quirks = 0;
    size_t portCount = 0;

    DKUSBHostController(struct tendril_hc *controller = nullptr,
                        struct znu_tendril_ctx *context = nullptr,
                        int type = 0,
                        uint32_t hcQuirks = 0,
                        size_t ports = 0)
        : hc(controller), ctx(context), hcType(type), quirks(hcQuirks), portCount(ports) {
        name = "DKUSBHostController";
    }

    bool isUSBHostController() const override { return true; }
    struct tendril_hc *getTendrilHC() const { return hc; }
    struct znu_tendril_ctx *getTendrilContext() const { return ctx; }
    size_t getPortCount() const { return portCount; }
};

class DKDevice : public DKService {
public:
    DKDevice() { name = "DKDevice"; }
    virtual const char *getDeviceType() const { return "Generic"; }
    virtual bool isUSBDevice() const { return false; }
};

class DKUSBDevice : public DKDevice {
public:
    struct tendril_device *tendrilDev = nullptr;
    uint8_t port = 0;
    uint8_t address = 0;
    DKUSBSpeed speed = DKUSBSpeed::Unknown;
    uint16_t vendorID = 0;
    uint16_t productID = 0;
    uint8_t deviceClass = 0;
    uint8_t deviceSubClass = 0;
    uint8_t deviceProtocol = 0;
    uint16_t hidKinds = 0;
    uint8_t hidBootProtocol = 0;

    DKUSBDevice(struct tendril_device *tdev = nullptr,
                uint8_t usbPort = 0,
                uint8_t usbAddress = 0,
                DKUSBSpeed usbSpeed = DKUSBSpeed::Unknown,
                uint16_t vid = 0,
                uint16_t pid = 0,
                uint8_t cls = 0,
                uint8_t subcls = 0,
                uint8_t proto = 0)
        : tendrilDev(tdev),
          port(usbPort),
          address(usbAddress),
          speed(usbSpeed),
          vendorID(vid),
          productID(pid),
          deviceClass(cls),
          deviceSubClass(subcls),
          deviceProtocol(proto) {
        name = "DKUSBDevice";
    }

    const char *getDeviceType() const override { return "USB"; }
    bool isUSBDevice() const override { return true; }
    struct tendril_device *getTendrilDevice() const { return tendrilDev; }
};

class DKDriver : public DKService {
protected:
    DKDevice *device = nullptr;

public:
    DKDriver() { name = "DKDriver"; }

    virtual bool attach(DKDevice *target) {
        if (!target) return false;
        device = target;
        return true;
    }

    virtual void detach() {
        device = nullptr;
    }

    DKPtr<DKDevice> getDevice() const { return DKPtr<DKDevice>(device); }
};

class DKUSBDriver : public DKDriver {
public:
    DKUSBDriver() { name = "DKUSBDriver"; }

    bool attach(DKDevice *target) override {
        if (!target || !target->isUSBDevice()) return false;
        return DKDriver::attach(target);
    }

    DKPtr<DKUSBDevice> getUSBDevice() const {
        return DKPtr<DKUSBDevice>(static_cast<DKUSBDevice *>(device));
    }
};

class DKResource : public DKObject {
public:
    virtual bool isValid() const = 0;
    virtual ~DKResource() = default;
};

struct DKDriverFactory {
    const char *driverName;
    DKService *(*create)();
    DKDriverFactory *next;
};
