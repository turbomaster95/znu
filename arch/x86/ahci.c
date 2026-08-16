#include <stdint.h>
#include <stdbool.h>
#include <page.h>
#include <pci.h>
#include <ahci.h>

extern uint64_t* kernel_pml4;

uintptr_t ahci_arch_virt_to_phys(const void* virt) {
#ifdef VIRT_TO_PHYS
    return VIRT_TO_PHYS((uintptr_t)virt);
#else
    return vmm_virt_to_phys(kernel_pml4, (uintptr_t)virt);
#endif
}

void* ahci_arch_phys_to_virt(uintptr_t phys) {
    return (void*)PHYS_TO_VIRT(phys);
}

void ahci_arch_cpu_relax(void) {
    asm volatile("pause");
}

uint32_t ahci_arch_mmio_read(volatile uint32_t* addr) {
    uint32_t val = *addr;
    asm volatile("" ::: "memory");
    return val;
}

void ahci_arch_mmio_write(volatile uint32_t* addr, uint32_t val) {
    *addr = val;
    asm volatile("" ::: "memory");
}

uint64_t ahci_arch_get_abar(void) {
    pci_device_t* dev = pci_find_class(0x01, 0x06, 0x01);
    if (!dev) return 0;

    pci_enable_busmaster(dev);

    uint32_t lo = pci_read_dword(dev->bus, dev->slot, dev->func, 0x24);
    uint32_t hi = pci_read_dword(dev->bus, dev->slot, dev->func, 0x28);
    return ((uint64_t)hi << 32) | (lo & 0xFFFFFFF0);
}
