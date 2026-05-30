#include <stdlib.h>
#include <cpuid.h>

void simd_init(void) {
    if (!cpu_has(SSE_SUPPORT)) {
        debugln("[simd] Basic SSE Support is unavailable on this cpu!");
        debugln("[simd] Some apps might not run properly or crash the system..");
        return;
    }

    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Clear EM
    cr0 |= (1 << 1);  // Set MP
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // Set OSFXSR
    cr4 |= (1 << 10); // Set OSXMMEXCPT
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
    debugln("[simd] Enabled Basic SSE/FPU Operations.");

    if (cpu_has(XSAVE_SUPPORT)) {
        cr4 |= (1 << 18); // Set OSXSAVE
        asm volatile("mov %0, %%cr4" : : "r"(cr4));

        uint32_t xcr0_eax = (1 << 0) | (1 << 1); // Always enable x87 and SSE

        if (cpu_has(AVX_SUPPORT)) {
            xcr0_eax |= (1 << 2); // Enable AVX state
        }

        uint32_t edx = 0;
        asm volatile("xsetbv" : : "a"(xcr0_eax), "d"(edx), "c"(0));
        debugln("[simd] Enabled XSAVE!");
    }
}
