#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>

bool fat32_init(void);

void fat32_ls_root(void);

bool fat32_read_file(
    const char* filename,
    void* out,
    uint32_t max_len,
    uint32_t* out_size
);

#endif
