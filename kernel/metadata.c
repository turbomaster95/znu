#include <stdint.h>
// This file contains metadata i wanted to put inside the znu kernel

__attribute__((section(".rodata.znumeta"), used, aligned(4)))
const char znu_metadata[] = "znu_version";
