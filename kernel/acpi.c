#define ACPI_USE_SYSTEM_CLIBRARY 0
#define ACPI_MACHINE_WIDTH 64
#define ACPI_TRACING_FEATURE 0

#include <stdint.h>
#include <stddef.h>
#include <page.h>
#include <limine.h>
#include <acpi.h>
#include <stdlib.h>

extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_hhdm_request hhdm_request;

/* --- Final Linker & Header Fixes --- */

// Use void* to avoid 'unknown type name' errors for internal ACPICA types
ACPI_STATUS AcpiUtOsiImplementation(void *WalkState) { return AE_OK; }
ACPI_STATUS AcpiUtGetInterface(ACPI_STRING a, void* b) { return AE_OK; }
ACPI_STATUS AcpiUtInstallInterface(ACPI_STRING a) { return AE_OK; }
ACPI_STATUS AcpiUtRemoveInterface(ACPI_STRING a) { return AE_OK; }
ACPI_STATUS AcpiUtUpdateInterfaces(UINT32 a) { return AE_OK; }
void AcpiUtInterfaceTerminate(void) {}

// Tracing stubs - simplified signatures
void AcpiExStartTraceMethod(void* a, void* b, void* c) {}
void AcpiExStopTraceMethod(void* a, void* b, void* c) {}
void AcpiExTraceArgs(void* a, void* b, void* c) {}
void AcpiExStartTraceOpcode(void* a, void* b) {}
void AcpiExStopTraceOpcode(void* a, void* b) {}

// Memory access for Table Manager
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
    void* ptr = AcpiOsMapMemory(Address, Width / 8);
    if (Width == 8)  *Value = *(volatile uint8_t*)ptr;
    else if (Width == 16) *Value = *(volatile uint16_t*)ptr;
    else if (Width == 32) *Value = *(volatile uint32_t*)ptr;
    else if (Width == 64) *Value = *(volatile uint64_t*)ptr;
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
    void* ptr = AcpiOsMapMemory(Address, Width / 8);
    if (Width == 8)  *(volatile uint8_t*)ptr = (uint8_t)Value;
    else if (Width == 16) *(volatile uint16_t*)ptr = (uint16_t)Value;
    else if (Width == 32) *(volatile uint32_t*)ptr = (uint32_t)Value;
    else if (Width == 64) *(volatile uint64_t*)ptr = (uint64_t)Value;
    return AE_OK;
}

// Execution and Overrides
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) { return AE_OK; }
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) { 
    *NewTable = NULL; return AE_OK; 
}
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewLength) { 
    *NewAddress = 0; return AE_OK; 
}
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal, ACPI_STRING *NewVal) { 
    *NewVal = NULL; return AE_OK; 
}

// Printing
void AcpiOsPrintf(const char *Format, ...) { }
void AcpiOsVprintf(const char *Format, va_list Args) { }

/* --- The Final Three Missing Symbols --- */

// This is required for AcpiEnterSleepState to actually function
ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    return AE_OK;
}

// These are internal ACPICA functions that seem to be missing from your library build
ACPI_STATUS AcpiUtInitializeInterfaces(void) {
    return AE_OK;
}


/* --- Local Assembly Helpers to avoid Header Conflicts --- */
static inline uint8_t _inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline uint16_t _inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline uint32_t _inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void _outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline void _outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline void _outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* --- ACPICA OSL Implementation --- */

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
    // If the address is already in the higher half (above hhdm_offset), 
    // don't add it again.
    if (PhysicalAddress >= hhdm_offset) {
        return (void*)PhysicalAddress;
    }
    void* virt = (void*)(PhysicalAddress + hhdm_offset);
    debugln("ACPI Map: Phys %p -> Virt %p", PhysicalAddress, virt);
    return (void*)(PhysicalAddress + hhdm_offset);
}


void AcpiOsUnmapMemory(void* where, ACPI_SIZE length) {}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    if (rsdp_request.response == NULL) return 0;
    
    uint64_t addr = (uint64_t)rsdp_request.response->address;
    
    // CONVERT TO PHYSICAL: Strip the HHDM offset before giving it to ACPICA
    if (addr >= 0xffff800000000000) {
        return (ACPI_PHYSICAL_ADDRESS)(addr - 0xffff800000000000);
    }
    debugln("OSL: AcpiOsGetRootPointer: %d", (ACPI_PHYSICAL_ADDRESS)addr);
    return (ACPI_PHYSICAL_ADDRESS)addr;
}


int alloc_count = 0;
void* AcpiOsAllocate(ACPI_SIZE Size) {
    alloc_count++;
    if (alloc_count % 100 == 0) debugln("OSL: %d allocations performed", alloc_count);
    return kmalloc(Size);
}

void AcpiOsFree(void* Memory) { kfree(Memory); }

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
    switch (Width) {
        case 8:  *Value = (UINT32)_inb(Address); break;
        case 16: *Value = (UINT32)_inw(Address); break;
        case 32: *Value = (UINT32)_inl(Address); break;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    switch (Width) {
        case 8:  _outb(Address, (uint8_t)Value); break;
        case 16: _outw(Address, (uint16_t)Value); break;
        case 32: _outl(Address, (uint32_t)Value); break;
    }
    return AE_OK;
}

/* --- Stubs --- */
ACPI_STATUS AcpiOsInitialize() { return AE_OK; }
ACPI_STATUS AcpiOsTerminate() { return AE_OK; }
ACPI_THREAD_ID AcpiOsGetThreadId(void) { return 1; }
ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info) { return AE_OK; }
void AcpiOsWaitEventsComplete(void) {}
void AcpiOsSleep(UINT64 Milliseconds) {}
void AcpiOsStall(UINT32 Microseconds) {
    // You have a calibrated LAPIC! Use it.
    // If 1ms = 63039 ticks, then 1us = ~63 ticks.
    uint64_t start = AcpiOsGetTimer();
    uint64_t ticks_to_wait = Microseconds * 63; 
    while (AcpiOsGetTimer() - start < ticks_to_wait) {
        asm volatile ("pause");
    }
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context) { return AE_OK; }
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) { return AE_OK; }

ACPI_STATUS AcpiOsCreateCache(char *CacheName, UINT16 ObjectSize, UINT16 MaxDepth, ACPI_CACHE_T **ReturnCache) {
    *ReturnCache = (ACPI_CACHE_T*)1; return AE_OK; 
}
ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T *Cache) { return AE_OK; }
ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T *Cache) { return AE_OK; }
void* AcpiOsAcquireObject(ACPI_CACHE_T *Cache) { return kmalloc(1024); } 
ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T *Cache, void *Object) { kfree(Object); return AE_OK; }

// Updated OSL Semaphore implementation
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle) {
    uint32_t* sem = kmalloc(sizeof(uint32_t));
    if (!sem) return AE_NO_MEMORY;
    *sem = InitialUnits;
    *OutHandle = (ACPI_SEMAPHORE)sem;
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
    if (!Handle) return AE_BAD_PARAMETER;
    // For single-core, we don't block, but we MUST update the count
    uint32_t* val = (uint32_t*)Handle;
    if (*val >= Units) {
        *val -= Units;
        return AE_OK;
    }
    // If it was 0, it would normally block here. 
    return AE_OK; 
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) { return AE_OK; }
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) { return AE_OK; }

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
    uint32_t* lock = kmalloc(sizeof(uint32_t));
    *lock = 0;
    *OutHandle = (ACPI_SPINLOCK)lock;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {}
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) { return 0; }
void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {}

UINT64 AcpiOsGetTimer(void) {
    uint32_t low, high;
    asm volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((UINT64)high << 32) | low;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 *Value, UINT32 Width) { return AE_NOT_IMPLEMENTED; }
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 Value, UINT32 Width) { return AE_NOT_IMPLEMENTED; }

void init_acpi() {
    debugln("Acpi Subsystem");
    if (ACPI_FAILURE(AcpiInitializeSubsystem())) return;
    debugln("Acpi Tables");
    if (ACPI_FAILURE(AcpiInitializeTables(NULL, 16, FALSE))) return;
    debugln("Acpi Load tables");
    if (ACPI_FAILURE(AcpiLoadTables())) return;
    debugln("Acpi Enable Subsystem");
    if (ACPI_FAILURE(AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))) return;
    debugln("Acpi Init Obj");
    if (ACPI_FAILURE(AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))) return;
}

void acpi_shutdown() {
    AcpiEnterSleepStatePrep(ACPI_STATE_S5);
    asm volatile ("cli");
    AcpiEnterSleepState(ACPI_STATE_S5);
    while(1) asm volatile ("hlt");
}

