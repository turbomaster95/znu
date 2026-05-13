#include <fat32.h>
#include <ahci.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <disk.h>

#define FAT32_SECTOR_SIZE 512
#define FAT32_EOC 0x0FFFFFF8

typedef struct __attribute__((packed)) {
    uint8_t  jump[3];
    uint8_t  oem[8];

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;

    uint16_t root_entries;
    uint16_t total_sectors16;
    uint8_t  media_type;
    uint16_t fat_size16;

    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;

    uint32_t fat_size32;
    uint16_t ext_flags;
    uint16_t fs_version;

    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;

    uint8_t  reserved0[12];

    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;

    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} FAT32_BPB;

typedef struct __attribute__((packed)) {
    uint8_t  name[11];
    uint8_t  attr;

    uint8_t  nt_reserved;
    uint8_t  creation_tenths;

    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;

    uint16_t cluster_high;
    uint16_t modified_time;
    uint16_t modified_date;

    uint16_t cluster_low;
    uint32_t size;
} FAT32_DIR_ENTRY;

static FAT32_BPB g_bpb;
static uint32_t g_fat_lba;
static uint32_t g_data_lba;
static uint32_t g_root_cluster;
static bool g_initialized = false;

static inline disk_t* disk(void) {
    return disk_get_boot();
}

/* ---------------- FAT HELPERS ---------------- */

static inline bool valid_cluster(uint32_t c) {
    return c >= 2 && c < FAT32_EOC;
}

static inline uint32_t cluster_to_lba(uint32_t cluster) {
    return g_data_lba +
        (cluster - 2) * g_bpb.sectors_per_cluster;
}

static uint32_t fat_next(uint32_t cluster) {
    if (!valid_cluster(cluster))
        return FAT32_EOC;

    uint32_t fat_offset = cluster * 4;
    uint32_t sector = g_fat_lba + (fat_offset / 512);
    uint32_t off = fat_offset % 512;

    uint8_t buf[512];

    if (!disk_read_sector(disk(), sector, buf))
        return FAT32_EOC;

    uint32_t val = *(uint32_t*)(buf + off);
    return val & 0x0FFFFFFF;
}

static bool read_cluster(uint32_t cluster, void* out) {
    if (!valid_cluster(cluster))
        return false;

    uint32_t lba = cluster_to_lba(cluster);

    for (uint32_t i = 0; i < g_bpb.sectors_per_cluster; i++) {
        if (!disk_read_sector(disk(), lba + i,
            (uint8_t*)out + i * FAT32_SECTOR_SIZE))
            return false;
    }

    return true;
}

bool fat32_init(void) {
    if (!disk_get_boot())
        return false;
    debugln("[fat32] disk=%p", disk_get_boot());

    uint8_t sector[512];

    if (!disk_read_sector(disk(), 0, sector))
        return false;

    memcpy(&g_bpb, sector, sizeof(g_bpb));

    if (g_bpb.bytes_per_sector != 512 || g_bpb.fat_size32 == 0)
        return false;

    g_fat_lba = g_bpb.reserved_sectors;
    g_data_lba = g_bpb.reserved_sectors +
        (g_bpb.fat_count * g_bpb.fat_size32);

    g_root_cluster = g_bpb.root_cluster;

    if (!valid_cluster(g_root_cluster))
        return false;

    g_initialized = true;

    debugln("[fat32] mounted root=%u", g_root_cluster);
    return true;
}

/* ---------------- NAME MATCH ---------------- */

static bool name_match(FAT32_DIR_ENTRY* e, const char* name) {
    char tmp[13] = {0};
    int p = 0;

    for (int i = 0; i < 8; i++) {
        if (e->name[i] == ' ') break;
        tmp[p++] = (e->name[i] >= 'A' && e->name[i] <= 'Z')
            ? e->name[i] + 32
            : e->name[i];
    }

    bool ext = false;
    for (int i = 8; i < 11; i++)
        if (e->name[i] != ' ') ext = true;

    if (ext) {
        tmp[p++] = '.';

        for (int i = 8; i < 11; i++) {
            if (e->name[i] == ' ') break;
            tmp[p++] = (e->name[i] >= 'A' && e->name[i] <= 'Z')
                ? e->name[i] + 32
                : e->name[i];
        }
    }

    return strcmp(tmp, name) == 0;
}

/* ---------------- READ FILE (FIXED SAFE VERSION) ---------------- */

bool fat32_read_file(
    const char* filename,
    void* out,
    uint32_t max_len,
    uint32_t* out_size
) {
    if (!g_initialized)
        return false;

    uint32_t cluster_size =
        g_bpb.sectors_per_cluster * 512;

    uint8_t* buf = kmalloc(cluster_size);
    if (!buf)
        return false;

    uint32_t cluster = g_root_cluster;

    while (valid_cluster(cluster)) {

        if (!read_cluster(cluster, buf)) {
            kfree(buf);
            return false;
        }

        FAT32_DIR_ENTRY* ent = (FAT32_DIR_ENTRY*)buf;
        int count = cluster_size / sizeof(FAT32_DIR_ENTRY);

        for (int i = 0; i < count; i++) {

            if (ent[i].name[0] == 0x00)
                goto done;

            if (ent[i].name[0] == 0xE5)
                continue;

            if (ent[i].attr == 0x0F)
                continue;

            if (!name_match(&ent[i], filename))
                continue;

            uint32_t c =
                (ent[i].cluster_high << 16) |
                ent[i].cluster_low;

            if (!valid_cluster(c)) {
                kfree(buf);
                return false;
            }

            uint32_t remaining = ent[i].size;
            uint8_t* dst = out;

            while (valid_cluster(c) && remaining && max_len) {

                if (!read_cluster(c, buf)) {
                    kfree(buf);
                    return false;
                }

                uint32_t to_copy =
                    (remaining < cluster_size) ? remaining : cluster_size;

                if (to_copy > max_len)
                    to_copy = max_len;

                memcpy(dst, buf, to_copy);

                dst += to_copy;
                remaining -= to_copy;
                max_len -= to_copy;

                c = fat_next(c);
            }

            if (out_size)
                *out_size = ent[i].size;

            kfree(buf);
            return true;
        }

        cluster = fat_next(cluster);
    }

done:
    kfree(buf);
    return false;
}
