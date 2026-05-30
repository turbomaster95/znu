#include <stdlib.h>
#include <stdint.h>
#include <cpuid.h>

#define EFER_MSR 0xC0000080
#define EFER_NXE (1ULL << 11)

bool nx_supported = false;

static int support_nx(void) {
    if (!cpu_has(NX_SUPPORT)) {
        return -1; // Potato PC doesn't even have NX bit humph.
    }
    return 0;
}

void enable_nx_bit(void) {
    if (support_nx()) {
	    uint64_t efer = rdmsr(EFER_MSR);
    
	    efer |= EFER_NXE;
    
	    wrmsr(EFER_MSR, (uint32_t)(efer & 0xFFFFFFFF), (uint32_t)(efer >> 32));
	    debugln("[nx] NX Bit support enabled!");
	    nx_supported = true;
    } else {
	    debugln("[nx] NX Bit isn't supported by this CPU!");
	    nx_supported = false;
    }
}
