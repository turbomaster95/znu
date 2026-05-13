#include <disk.h>
#include <ahci.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

disk_t g_disks[MAX_DISKS];
int g_disk_count = 0;

static disk_t* g_boot_disk = NULL;

bool disk_read_sector(disk_t* d, uint64_t lba, void* buf) {
    if (!d || !d->present)
        return false;

    return ahci_read_sector(d->port, lba, buf);
}

bool disk_write_sector(disk_t* d, uint64_t lba, void* buf) {
    if (!d || !d->present)
        return false;

    return ahci_write_sector(d->port, lba, buf);
}

void disk_register_ahci_port(int port) {
    if (g_disk_count >= MAX_DISKS)
        return;

    disk_t* d = &g_disks[g_disk_count++];

    memset(d, 0, sizeof(disk_t));

    d->port = port;
    d->present = true;

    snprintf(d->name, DISK_NAME_LEN, "ahci%d", port);

    d->sector_size = 512;

    debugln("[disk] registered %s", d->name);

    if (!g_boot_disk)
        g_boot_disk = d;
}

void disk_init(void) {
    g_disk_count = 0;
    g_boot_disk = NULL;

    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (ahci_port_is_present(i)) {
            disk_register_ahci_port(i);
        }
    }

    debugln("[disk] total disks: %d", g_disk_count);
}

disk_t* disk_get_boot(void) {
    return g_boot_disk;
}
