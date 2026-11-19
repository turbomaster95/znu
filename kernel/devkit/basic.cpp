
#include <devkit/devkit.h>
#include <devkit/core.hpp>
#include <string.h>

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
            p_info("DevKit", "matched '%s' -> '%s' (score=%d)", stub->getName(), bestFactory->driverName, bestScore);
        } else {
            debugln("[devkit] driver '%s' failed to start", bestFactory->driverName);
        }
        bestInstance->release();
    }
}

static unsigned int dk_count_children(DKService *node) {
    unsigned int count = 0;
    for (DKService *child = node->getFirstChild(); child != nullptr; child = child->getNextSibling()) count++;
    return count;
}

static unsigned int dk_count_bars(const DKPCIDevice *device) {
    unsigned int count = 0;
    for (unsigned int i = 0; i < 6; i++) {
        const DKPCIResource *bar = device->getBAR(i);
        if (bar && bar->isValid()) count++;
    }
    return count;
}

static void dk_make_prefix(char *out, const char *parent, bool last) {
    unsigned int i = 0;
    while (parent[i] && i < 112) {
        out[i] = parent[i];
        i++;
    }
    const char *branch = last ? "`-- " : "|-- ";
    for (unsigned int j = 0; branch[j] && i < 124; j++) out[i++] = branch[j];
    out[i] = '\0';
}

static void dk_make_child_prefix(char *out, const char *parent, bool last) {
    unsigned int i = 0;
    while (parent[i] && i < 112) {
        out[i] = parent[i];
        i++;
    }
    const char *branch = last ? "    " : "|   ";
    for (unsigned int j = 0; branch[j] && i < 124; j++) out[i++] = branch[j];
    out[i] = '\0';
}

static void dk_print_bar(const char *prefix, const DKPCIResource *bar, unsigned int index, bool last) {
    char linePrefix[128];
    const char *type = "Unknown";
    const char *unit = "B";
    unsigned long long size = (unsigned long long)bar->size;

    dk_make_prefix(linePrefix, prefix, last);

    if (bar->type == DKPCIResource::MMIO) type = "MMIO";
    else if (bar->type == DKPCIResource::IO) type = "I/O";

    if (bar->size >= 1073741824ULL && bar->size % 1073741824ULL == 0) {
        size = (unsigned long long)(bar->size / 1073741824ULL);
        unit = "GiB";
    } else if (bar->size >= 1048576ULL && bar->size % 1048576ULL == 0) {
        size = (unsigned long long)(bar->size / 1048576ULL);
        unit = "MiB";
    } else if (bar->size >= 1024ULL && bar->size % 1024ULL == 0) {
        size = (unsigned long long)(bar->size / 1024ULL);
        unit = "KiB";
    }

    debugln("%sBAR%u %-4s 0x%016llX (%llu %s, %u-bit%s)", linePrefix, index, type, (unsigned long long)bar->base, size, unit, (unsigned int)bar->width, bar->prefetchable ? ", prefetchable" : "");
}

static void dk_print_tree(DKService *node, const char *parentPrefix, bool isLast, bool isRoot) {
    if (!node) return;

    char linePrefix[128];
    char childPrefix[128];

    if (isRoot) {
        linePrefix[0] = '\0';
        debugln("DKService");
    } else {
        dk_make_prefix(linePrefix, parentPrefix, isLast);

        if (strcmp(node->getName(), "DKPCIDevice") == 0) {
            DKPCIDevice *device = static_cast<DKPCIDevice *>(node);
            debugln("%sPCI %02X:%02X.%u [%04X:%04X] IRQ %u", linePrefix, device->bus, device->slot, device->func, device->vendorID, device->deviceID, (unsigned int)device->irq);
        } else {
            debugln("%s%s", linePrefix, node->getName());
        }
    }

    if (isRoot) {
        childPrefix[0] = '\0';
    } else {
        dk_make_child_prefix(childPrefix, parentPrefix, isLast);
    }

    unsigned int barCount = 0;
    unsigned int childCount = dk_count_children(node);
    DKPCIDevice *pciDevice = nullptr;

    if (!isRoot && strcmp(node->getName(), "DKPCIDevice") == 0) {
        pciDevice = static_cast<DKPCIDevice *>(node);
        barCount = dk_count_bars(pciDevice);
    }

    unsigned int totalEntries = barCount + childCount;
    unsigned int currentEntry = 0;

    if (pciDevice) {
        for (unsigned int i = 0; i < 6; i++) {
            const DKPCIResource *bar = pciDevice->getBAR(i);
            if (!bar || !bar->isValid()) continue;
            currentEntry++;
            dk_print_bar(childPrefix, bar, i, currentEntry == totalEntries);
        }
    }

    for (DKService *child = node->getFirstChild(); child != nullptr; child = child->getNextSibling()) {
        currentEntry++;
        dk_print_tree(child, childPrefix, currentEntry == totalEntries, false);
    }
}

extern "C" void devkit_init(void) {
    if (!gRootService) {
        gRootService = new DKService();
        p_info("DevKit", "root registry initialized");
    }
}

extern "C" void devkit_publish_pci_device(uint8_t bus, uint8_t slot, uint8_t func, uint16_t vendor_id, uint16_t device_id, const devkit_pci_bar_t bars[6], uint8_t irq) {
    if (!gRootService) devkit_init();

    DKPCIDevice *stub = new DKPCIDevice(bus, slot, func, vendor_id, device_id, bars, irq);
    gRootService->attachChild(stub);
    dk_match_and_attach(stub);
    stub->release();
}

extern "C" void devkit_dump_registry(void) {
    debugln("=== DevKit Device Registry ===");
    if (!gRootService) {
        debugln("(registry not initialized)");
        return;
    }
    dk_print_tree(gRootService, "", true, true);
}
