#include <stdlib.h>

inline uint64_t read_cr4(void) {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

inline void write_cr4(uint64_t cr4) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
}

void disable_smap(void) {
    uint64_t cr4 = read_cr4();
    cr4 &= ~(1ULL << 21); // Bit 21 is SMAP
    write_cr4(cr4);
}
