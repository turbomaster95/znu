#include <devkit/devkit.h>
#include <devkit/core.hpp>

static DKService *gRootService = nullptr;
static DKDriverFactory *gDriverList = nullptr;

void dk_register_driver(DKDriverFactory *factory) {
    factory->next = gDriverList;
    gDriverList = factory;
}

static void dk_match_and_attach(DKService *stub) {
    DKDriverFactory *bestFactory = nullptr;
    DKService *bestInstance = nullptr;
    int bestScore = 0;

    for (DKDriverFactory *f = gDriverList; f != nullptr; f = f->next) {
        DKService *candidate = f->create();
        if (!candidate || !candidate->init()) {
            if (candidate) candidate->release();
            continue;
        }

        int score = candidate->probe(stub);
        if (score > bestScore) {
            if (bestInstance) bestInstance->release();
            bestScore = score;
            bestFactory = f;
            bestInstance = candidate;
        } else {
            candidate->release();
        }
    }

    if (bestInstance && bestScore > 0) {
        stub->attachChild(bestInstance);
        if (bestInstance->start(stub)) {
            debugln("[devkit] matched '%s' -> '%s' (score=%d)",
                    stub->getName(), bestFactory->driverName, bestScore);
        } else {
            debugln("[devkit] driver '%s' failed to start", bestFactory->driverName);
        }
        bestInstance->release(); // Parent stub now holds the reference
    }
}

static void dk_print_tree(DKService *node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) {
        debugln("  "); // Or print indentation without newline if your logger supports it
    }
    debugln("|-- %s", node->getName());

    for (DKService *child = node->getFirstChild(); child != nullptr; child = child->getNextSibling()) {
        dk_print_tree(child, depth + 1);
    }
}

extern "C" void devkit_init(void) {
    if (!gRootService) {
        gRootService = new DKService();
        debugln("[devkit] root registry initialized");
    }
}

extern "C" void devkit_publish_pci_device(uint8_t bus, uint8_t slot, uint8_t func,
                                          uint16_t vendor_id, uint16_t device_id,
                                          uint32_t bar0, uint8_t irq) {
    if (!gRootService) devkit_init();

    DKPCIDevice *stub = new DKPCIDevice(bus, slot, func, vendor_id, device_id, bar0, irq);
    gRootService->attachChild(stub);
    dk_match_and_attach(stub);
    stub->release(); // Root now holds the reference
}

extern "C" void devkit_dump_registry(void) {
    debugln("=== DevKit I/O Registry ===");
    dk_print_tree(gRootService, 0);
}
