#include <stdint.h>
// This file contains metadata i wanted to put inside the znu kernel

typedef struct {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
} elf_note_hdr;

__attribute__((section(".note.znu"), used, aligned(4)))
const struct {
    elf_note_hdr hdr;
    char name[4];
    char desc[128];
} znu_metadata = {
    .hdr = {
        .namesz = 4,
        .descsz = 128,
        .type = 1
    },
    .name = "znu",
    .desc = "Written by Deva with Android and MacOS."
};
