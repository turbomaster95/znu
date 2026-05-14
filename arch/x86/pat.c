#include <stdint.h>

#define MSR_PAT 0x277

void pat_init(void) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_PAT));
    high &= ~0x7; 
    high |= 0x01;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(MSR_PAT));
}
