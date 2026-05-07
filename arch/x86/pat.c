#include <stdint.h>

#define MSR_PAT 0x277

void pat_init(void) {
    uint32_t low, high;
    
    // Read the current PAT MSR
    // low contains PA0-PA3, high contains PA4-PA7
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_PAT));

    // We modify PA4 (the first slot in 'high')
    // A value of 0x01 is Write-Combining (WC)
    // Clear the bottom 3 bits of 'high' and set them to 0x01
    high &= ~0x7; 
    high |= 0x01;

    // Write it back
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(MSR_PAT));
}
