#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdbool.h>

#define DISK_NAME_LEN 32

typedef struct disk {
    char name[DISK_NAME_LEN];

    // AHCI binding
    int port;

    uint64_t sector_count;
    uint32_t sector_size;

    // state
    bool present;
    bool boot_disk;

} disk_t;

#define MAX_DISKS 16

extern disk_t g_disks[MAX_DISKS];
extern int g_disk_count;

disk_t* disk_get_boot(void);
disk_t* disk_get(int idx);

bool disk_read_sector(disk_t* d, uint64_t lba, void* buf);
bool disk_write_sector(disk_t* d, uint64_t lba, void* buf);
void disk_register_ahci_port(int port);
void disk_init(void);

#endif
