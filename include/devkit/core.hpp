#pragma once

#include <stdint.h>
#include <stddef.h>

extern "C" {
#include <stdlib.h>
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

    void attachChild(DKService *child) {
        child->retain();
        child->provider = this;
        child->nextSibling = firstChild;
        firstChild = child;
    }

    const char *getName() const { return name; }
    DKService *getFirstChild() const { return firstChild; }
    DKService *getNextSibling() const { return nextSibling; }
};

class DKPCIDevice : public DKService {
public:
    uint8_t bus, slot, func;
    uint16_t vendorID, deviceID;
    uint32_t bar0;
    uint8_t irq;

    DKPCIDevice(uint8_t b, uint8_t s, uint8_t f,
                uint16_t vid, uint16_t did, uint32_t b0, uint8_t i)
        : bus(b), slot(s), func(f), vendorID(vid), deviceID(did), bar0(b0), irq(i) {
        name = "DKPCIDevice";
    }
};

struct DKDriverFactory {
    const char *driverName;
    DKService *(*create)();
    DKDriverFactory *next;
};
