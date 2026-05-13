#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <disk.h>

typedef struct {
    uint32_t first_fat_sector;
    uint32_t sectors_per_cluster;
    uint32_t first_data_sector;
} fat32_info_t;

bool fat32_init(void);
bool fat32_init_on_disk(disk_t* d);
void fat32_ls_root(void);

bool fat32_read_file(
    const char* filename,
    void* out,
    uint32_t max_len,
    uint32_t* out_size
);

#endif
