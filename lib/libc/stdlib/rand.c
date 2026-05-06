#include <stdlib.h>
#include <stdint.h>

static uint64_t state = 1;

void srand(unsigned int seed) {
    state = seed;
}

int rand(void) {
    uint64_t x = state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state = x;
    return (int)(x & 0x7FFFFFFF); // Return positive 31-bit integer
}

int rdrand64(uint64_t* val) {
    unsigned char ok;
    __asm__ volatile ("rdrand %0; setc %1" 
                      : "=r" (*val), "=qm" (ok));
    return (int)ok;
}

void seed_from_hardware(void) {
    uint32_t eax, ebx, ecx, edx;
    uint64_t combined_entropy = 0;

    // CPUID call: leaf 1
    __asm__ volatile ("cpuid"
                      : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(1));

    int has_rdrand = (ecx & (1 << 30));

    for (int i = 0; i < 10; i++) {
        uint64_t temp = 0;
        if (has_rdrand && rdrand64(&temp)) {
            combined_entropy ^= temp;
        } else {
            // Fallback to RDTSC (Time Stamp Counter)
            uint32_t lo, hi;
            __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
            combined_entropy ^= ((uint64_t)hi << 32) | lo;
        }
    }

    srand((unsigned int)combined_entropy);
}
