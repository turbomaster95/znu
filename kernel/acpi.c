#define ACPI_USE_SYSTEM_CLIBRARY 0
#define ACPI_MACHINE_WIDTH 64

#include <stdint.h>
#include <stddef.h>
#include <page.h>

#undef __linux__
#undef __linux

#include <acpi.h>

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}


void* AcpiOsAllocate(ACPI_SIZE Size) {
    return kmalloc(Size);
}

void AcpiOsFree(void* Memory) {
    kfree(Memory);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void* LogicalAddress, ACPI_PHYSICAL_ADDRESS* PhysicalAddress) {
    if (!LogicalAddress || !PhysicalAddress) {
        return AE_BAD_PARAMETER;
    }

    // Use your VIRT_TO_PHYS macro from page.h
    *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)VIRT_TO_PHYS(LogicalAddress);
    
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, uint32_t *Value, uint32_t Width) {
    switch (Width) {
        case 8:  *Value = (uint32_t)inb(Address); break;
        case 16: *Value = (uint32_t)inw(Address); break;
        case 32: *Value = (uint32_t)inl(Address); break;
        default: return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, uint32_t Value, uint32_t Width) {
    if (Width == 8)      outb(Address, Value);
    else if (Width == 16) outw(Address, Value);
    else if (Width == 32) outl(Address, Value);
    return AE_OK;
}

