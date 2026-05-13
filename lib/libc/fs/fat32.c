#include <fat32.h>
#include <ahci.h>
#include <string.h>
#include <stdlib.h>
#include <page.h>
#include <stdio.h>
#include <stdint.h>
#include <syscall.h>
#include <stdbool.h>
#include <disk.h>
#include <vfs.h>
#include <ctype.h>

#define FAT32_SECTOR_SIZE 512
#define FAT32_EOC 0x0FFFFFF8

static fat32_info_t g_fat_info;

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
uint32_t g_root_cluster;
static disk_t* g_fat_disk = NULL;
static bool g_initialized = false;

/*
 * A single DMA-safe 512-byte scratch sector.
 * AHCI requires the target buffer to be in the HHDM so virt_to_phys()
 * can trivially compute its physical address.  Stack buffers and
 * kmalloc() heap buffers are NOT guaranteed to be in the HHDM, so we
 * allocate one page with palloc_zero() at init time and reuse it for
 * every sector read that does not already supply a palloc'd HHDM buffer.
 */
static uint8_t* g_dma_sector = NULL; /* virtual (HHDM) pointer, set by fat32_init_on_disk */


static inline disk_t* disk(void) {
    return g_fat_disk;
}

static inline bool valid_cluster(uint32_t c) {
    return c >= 2 && c < FAT32_EOC;
}

uint32_t cluster_to_lba(uint32_t cluster) {
    return g_data_lba + ((cluster - 2) * g_fat_info.sectors_per_cluster);
}

static void fat32_format_name(const char* fat_name, char* dest) {
    int i, j;
    // Copy name part
    for (i = 0; i < 8 && fat_name[i] != ' '; i++) {
        dest[i] = fat_name[i];
    }
    // Add dot if there's an extension
    if (fat_name[8] != ' ') {
        dest[i++] = '.';
        for (j = 0; j < 3 && fat_name[8+j] != ' '; j++) {
            dest[i++] = fat_name[8+j];
        }
    }
    dest[i] = '\0';
}

static uint32_t fat_next(uint32_t cluster) {
    if (!valid_cluster(cluster))
        return FAT32_EOC;

    uint32_t fat_offset = cluster * 4;
    uint32_t sector = g_fat_lba + (fat_offset / 512);
    uint32_t off = fat_offset % 512;

    if (!disk_read_sector(disk(), sector, g_dma_sector))
        return FAT32_EOC;

    uint32_t val = *(uint32_t*)(g_dma_sector + off);
    return val & 0x0FFFFFFF;
}

static bool read_cluster(uint32_t cluster, void* out) {
    if (!valid_cluster(cluster))
        return false;

    uint32_t lba = cluster_to_lba(cluster);

    for (uint32_t i = 0; i < g_bpb.sectors_per_cluster; i++) {
        if (!disk_read_sector(disk(), lba + i, g_dma_sector))
            return false;
        memcpy((uint8_t*)out + i * FAT32_SECTOR_SIZE, g_dma_sector, FAT32_SECTOR_SIZE);
    }

    return true;
}

bool fat32_init(void) {
    if (!disk_get_boot())
        return false;
    debugln("[fat32] disk=%p", disk_get_boot());

    if (!g_dma_sector) {
        void* p = palloc_zero();
        if (!p) return false;
        g_dma_sector = (uint8_t*)phys_to_virt((uintptr_t)p);
    }

    if (!disk_read_sector(disk(), 0, g_dma_sector))
        return false;

    // Copy the raw sector into your BPB structure
    memcpy(&g_bpb, g_dma_sector, sizeof(g_bpb));

    // Validation
    if (g_bpb.bytes_per_sector != 512 || g_bpb.fat_size32 == 0) {
        debugln("g_bpb.bytes_per_sector or g_bpb.fat_size32 err");
        return false;
    }

    g_fat_info.sectors_per_cluster = g_bpb.sectors_per_cluster;
    g_fat_info.first_fat_sector = g_bpb.reserved_sectors;
    g_fat_info.first_data_sector = g_bpb.reserved_sectors +
                                   (g_bpb.fat_count * g_bpb.fat_size32);

    // Keep your existing legacy globals if other parts of the driver use them
    g_fat_lba = g_fat_info.first_fat_sector;
    g_data_lba = g_fat_info.first_data_sector;
    g_root_cluster = g_bpb.root_cluster;

    if (!valid_cluster(g_root_cluster)) {
        debugln("g_root_cluster not valid cluster?");
        return false;
    }

    g_initialized = true;

    debugln("[fat32] mounted root=%u SPC=%u", g_root_cluster, g_fat_info.sectors_per_cluster);
    return true;
}

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

    //uint8_t* buf = kmalloc(cluster_size);
    static uint8_t debug_buf[8192];
    uint8_t* buf = debug_buf;
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


bool fat32_init_on_disk(disk_t* d) {
    if (!d) return false;
    debugln("[fat32-debug] starting mount for dev: %s", d->name);
    g_fat_disk = d;

    // 1. Allocate the permanent DMA-safe sector scratch buffer.
    //    This page is kept for the lifetime of the driver — every function
    //    that reads a single sector reuses it instead of using the stack.
    if (!g_dma_sector) {
        void* p = palloc_zero();
        if (!p) return false;
        g_dma_sector = (uint8_t*)phys_to_virt((uintptr_t)p);
    }

    // 2. Read Boot Sector (LBA 0)
    if (!disk_read_sector(d, 0, g_dma_sector)) {
        debugln("[fat32] disk_read_sector failed at LBA 0");
        return false;
    }

    // 3. Copy to global BPB
    memcpy(&g_bpb, g_dma_sector, sizeof(FAT32_BPB));

    debugln("[fat32] BPB Loaded. OEM: %.8s", g_bpb.oem);

    if (g_bpb.bytes_per_sector != 512 || g_bpb.fat_size32 == 0) {
        debugln("[fat32] Error: Not a valid FAT32 volume");
        return false;
    }

    g_fat_info.sectors_per_cluster = g_bpb.sectors_per_cluster;
    g_fat_info.first_fat_sector = g_bpb.reserved_sectors;
    g_fat_info.first_data_sector = g_bpb.reserved_sectors + (g_bpb.fat_count * g_bpb.fat_size32);

    // 4. Calculate LBA positions for internal driver use
    g_fat_lba = g_fat_info.first_fat_sector;
    g_data_lba = g_fat_info.first_data_sector;
    g_root_cluster = g_bpb.root_cluster;

    g_initialized = true;

    // 5. Read Root Directory (Debug listing)
    uint32_t root_lba = g_data_lba + (g_root_cluster - 2) * g_fat_info.sectors_per_cluster;

    debugln("[fat32] Root Dir LBA: %u", root_lba);

    if (disk_read_sector(d, root_lba, g_dma_sector)) {
        FAT32_DIR_ENTRY* debug_ent = (FAT32_DIR_ENTRY*)g_dma_sector;
        for (int i = 0; i < 16; i++) {
            if (debug_ent[i].name[0] == 0x00) break;
            if (debug_ent[i].name[0] == 0xE5) continue;
            if (debug_ent[i].attr == 0x0F) continue;

            debugln("[fat32-debug] Found: %.11s | Size: %u | Cluster: %u",
                    debug_ent[i].name,
                    debug_ent[i].size,
                    (uint32_t)debug_ent[i].cluster_low | ((uint32_t)debug_ent[i].cluster_high << 16));
        }
    }

    debugln("[fat32] Mount successful. SPC: %u", g_fat_info.sectors_per_cluster);
    return true;
}

static int fat32_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && (tolower(*s1) == tolower(*s2))) {
        s1++;
        s2++;
    }
    return (unsigned char)tolower(*s1) - (unsigned char)tolower(*s2);
}

bool fat32_compare_name(const char* name, const char* fat_name) {
    char clean_name[12];
    int idx = 0;
    
    // Copy the 8-char name, stop at space
    for (int i = 0; i < 8; i++) {
        if (fat_name[i] == ' ') break;
        clean_name[idx++] = fat_name[i];
    }
    
    // Add dot and extension if it exists
    if (fat_name[8] != ' ') {
        clean_name[idx++] = '.';
        for (int i = 8; i < 11; i++) {
            if (fat_name[i] == ' ') break;
            clean_name[idx++] = fat_name[i];
        }
    }
    clean_name[idx] = '\0';

    // Case-insensitive comparison (FAT32 is usually uppercase)
    return fat32_strcasecmp(name, clean_name) == 0;
}
uint32_t get_fat_entry(uint32_t cluster);

vfs_node_t* fat32_vfs_find(vfs_node_t* parent, const char* name) {
    /* parent->data stores the cluster number of the directory */
    uint32_t cluster = (uint32_t)parent->data;

    /* Allocate one DMA-safe sector buffer */
    void* dma_phys = palloc_zero();
    if (!dma_phys) return NULL;
    uint8_t* sector_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_phys);

    vfs_node_t* found_node = NULL;

    /* Walk every cluster in the directory chain */
    while (valid_cluster(cluster) && !found_node) {
        uint32_t lba = cluster_to_lba(cluster);

        /* Walk every sector in this cluster */
        for (uint32_t s = 0; s < g_fat_info.sectors_per_cluster && !found_node; s++) {
            if (!disk_read_sector(g_fat_disk, lba + s, sector_buf)) {
                pfree(dma_phys);
                return NULL;
            }

            FAT32_DIR_ENTRY* entries = (FAT32_DIR_ENTRY*)sector_buf;
            int entries_per_sector = 512 / sizeof(FAT32_DIR_ENTRY);

            for (int i = 0; i < entries_per_sector; i++) {
                if (entries[i].name[0] == 0x00) goto done; /* end of directory */
                if (entries[i].name[0] == 0xE5) continue;  /* deleted */
                if ((entries[i].attr & 0x0F) == 0x0F) continue; /* LFN */

                if (fat32_compare_name(name, (const char*)entries[i].name)) {
                    int type = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
                    found_node = vfs_create_node(name, type);
                    if (found_node) {
                        found_node->data = (uintptr_t)((uint32_t)entries[i].cluster_low |
                                           ((uint32_t)entries[i].cluster_high << 16));
                        found_node->size = entries[i].size;
                        found_node->ops  = parent->ops;
                    }
                    break;
                }
            }
        }

        cluster = get_fat_entry(cluster);
    }

done:
    pfree(dma_phys);
    return found_node;
}

uint32_t get_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat_info.first_fat_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (!disk_read_sector(g_fat_disk, fat_sector, g_dma_sector)) {
        debugln("[fat32] Failed to read FAT sector %u", fat_sector);
        return 0x0FFFFFF7; // Return a "Bad Cluster" marker
    }

    uint32_t next_cluster = *(uint32_t*)(g_dma_sector + ent_offset);

    return next_cluster & 0x0FFFFFFF;
}

int fat32_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (g_fat_info.sectors_per_cluster == 0) {
        debugln("[FAT32] ERROR: Geometry not initialized! sectors_per_cluster is 0.");
        return -1;
    }
    if (!g_fat_disk) {
        debugln("[FAT32] ERROR: g_fat_disk is NULL.");
        return -1;
    }

    uint32_t cluster = (uint32_t)node->data;
    uint32_t bytes_per_cluster = g_fat_info.sectors_per_cluster * 512;

    /* Skip clusters that are entirely before the requested offset */
    size_t cluster_byte_offset = offset;
    while (cluster_byte_offset >= bytes_per_cluster && valid_cluster(cluster)) {
        cluster = get_fat_entry(cluster);
        cluster_byte_offset -= bytes_per_cluster;
    }

    /* Allocate a single DMA-safe sector buffer (page-aligned, in the HHDM) */
    void* dma_phys = palloc_zero();
    if (!dma_phys) return -1;
    uint8_t* dma_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_phys);

    uint8_t* dst = (uint8_t*)buf;
    size_t total_read = 0;

    while (total_read < size && valid_cluster(cluster)) {
        uint32_t lba = cluster_to_lba(cluster);

        for (uint32_t i = 0; i < g_fat_info.sectors_per_cluster && total_read < size; i++) {
            /* Read this sector into the DMA buffer */
            if (!disk_read_sector(g_fat_disk, lba + i, dma_buf)) {
                debugln("[fat32] Read failed at cluster %u, LBA %u", cluster, lba + i);
                pfree(dma_phys);
                return (int)total_read;
            }

            /* On the first sector of the read we may need to skip intra-sector bytes */
            size_t sector_start = 0;
            if (cluster_byte_offset > 0) {
                sector_start = cluster_byte_offset < 512 ? cluster_byte_offset : 512;
                cluster_byte_offset -= sector_start;
            }

            size_t to_copy = 512 - sector_start;
            if (to_copy > size - total_read)
                to_copy = size - total_read;

            memcpy(dst + total_read, dma_buf + sector_start, to_copy);
            total_read += to_copy;
        }

        cluster = get_fat_entry(cluster);
    }

    pfree(dma_phys);
    return (int)total_read;
}

int fat32_vfs_readdir(vfs_node_t* node, uint32_t start_index, void* buf, size_t count) {
    uint32_t cluster = (uint32_t)node->data;

    void* dma_phys = palloc_zero();
    if (!dma_phys) return -1;
    uint8_t* sector_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_phys);

    znu_dirent_t* user_ents = (znu_dirent_t*)buf;
    size_t max_ents = count / sizeof(znu_dirent_t);
    int found_count = 0; /* total valid entries seen so far (for skip logic) */
    int user_idx = 0;    /* entries written into user buf */
    bool done = false;

    while (valid_cluster(cluster) && !done && (size_t)user_idx < max_ents) {
        uint32_t lba = cluster_to_lba(cluster);

        for (uint32_t s = 0; s < g_fat_info.sectors_per_cluster && !done && (size_t)user_idx < max_ents; s++) {
            if (!disk_read_sector(g_fat_disk, lba + s, sector_buf)) {
                pfree(dma_phys);
                return -1;
            }

            FAT32_DIR_ENTRY* entries = (FAT32_DIR_ENTRY*)sector_buf;
            int entries_per_sector = 512 / sizeof(FAT32_DIR_ENTRY);

            for (int i = 0; i < entries_per_sector; i++) {
                if (entries[i].name[0] == 0x00) { done = true; break; }
                if (entries[i].name[0] == 0xE5) continue;
                if ((entries[i].attr & 0x0F) == 0x0F) continue;

                if (found_count < (int)start_index) {
                    found_count++;
                    continue;
                }

                if ((size_t)user_idx < max_ents) {
                    fat32_format_name((char*)entries[i].name, user_ents[user_idx].name);
                    user_ents[user_idx].type = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
                    user_ents[user_idx].size = entries[i].size;
                    user_idx++;
                }
                found_count++;
            }
        }

        cluster = get_fat_entry(cluster);
    }

    pfree(dma_phys);
    return user_idx * sizeof(znu_dirent_t);
}

vfs_ops_t fat32_ops = {
    .read = fat32_vfs_read,
    .write = NULL,
    .readdir = fat32_vfs_readdir,
    .find_node = fat32_vfs_find
};
