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

    // Copy the raw sector into your BPB structure
    memcpy(&g_bpb, sector, sizeof(g_bpb));

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

    // 1. Allocate a DMA-safe page
    void* dma_buffer = palloc_zero();
    if (!dma_buffer) return false;

    // Map physical to virtual (HHDM)
    uint8_t* sector = (uint8_t*)phys_to_virt((uintptr_t)dma_buffer);

    // 2. Read Boot Sector (LBA 0)
    if (!disk_read_sector(d, 0, sector)) {
        debugln("[fat32] disk_read_sector failed at LBA 0");
        pfree(dma_buffer);
        return false;
    }

    // 3. Copy to global BPB
    memcpy(&g_bpb, sector, sizeof(FAT32_BPB));

    debugln("[fat32] BPB Loaded. OEM: %.8s", g_bpb.oem);

    if (g_bpb.bytes_per_sector != 512 || g_bpb.fat_size32 == 0) {
        debugln("[fat32] Error: Not a valid FAT32 volume");
        pfree(dma_buffer);
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

    // 5. Read Root Directory (Debug)
    uint32_t root_lba = g_data_lba + (g_root_cluster - 2) * g_fat_info.sectors_per_cluster;

    debugln("[fat32] Root Dir LBA: %u", root_lba);

    if (disk_read_sector(d, root_lba, sector)) {
        FAT32_DIR_ENTRY* debug_ent = (FAT32_DIR_ENTRY*)sector;
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

    pfree(dma_buffer);
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

vfs_node_t* fat32_vfs_find(vfs_node_t* parent, const char* name) {
    // parent->data stores the cluster number of the directory
    uint32_t cluster = (uint32_t)parent->data;
    
    // Allocate a buffer to read the directory cluster
    void* dma_buffer = palloc_zero();
    uint8_t* sector_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_buffer);
    
    uint32_t lba = cluster_to_lba(cluster);
    
    if (!disk_read_sector(g_fat_disk, lba, sector_buf)) {
        pfree(dma_buffer);
        return NULL;
    }

    FAT32_DIR_ENTRY* entries = (FAT32_DIR_ENTRY*)sector_buf;
    vfs_node_t* found_node = NULL;

    // Loop through the 16 entries in this sector
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5 || (entries[i].attr & 0x0F) == 0x0F) continue;

        // FAT32 names are space-padded (e.g., "HELLO      ")
        // We need a helper to compare "HELLO" with the padded name
        if (fat32_compare_name(name, (const char*)entries[i].name)) {
            int type = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
            found_node = vfs_create_node(name, type);
            
            if (found_node) {
                // Store the starting cluster in 'data' and file size in 'size'
                found_node->data = (uintptr_t)((uint32_t)entries[i].cluster_low | ((uint32_t)entries[i].cluster_high << 16));
                found_node->size = entries[i].size;
                found_node->ops = parent->ops; // Inherit FAT32 operations
            }
            break;
        }
    }

    pfree(dma_buffer);
    return found_node;
}

uint32_t get_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat_info.first_fat_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    uint8_t buffer[512];
    if (!disk_read_sector(g_fat_disk, fat_sector, buffer)) {
        debugln("[fat32] Failed to read FAT sector %u", fat_sector);
        return 0x0FFFFFF7; // Mark as bad cluster
    }

    uint32_t next_cluster = *(uint32_t*)(&buffer[ent_offset]);

    return next_cluster & 0x0FFFFFFF;
}

int fat32_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (g_fat_info.sectors_per_cluster == 0) {
        debugln("[FAT32] ERROR: Geometry not initialized! sectors_per_cluster is 0.");
        return -1;
    }
    uint32_t cluster = (uint32_t)node->data;
    uint32_t bytes_per_cluster = g_fat_info.sectors_per_cluster * 512;
    uint32_t total_read = 0;

    while (total_read < size && cluster < 0x0ffffff8) {
        uint32_t lba = cluster_to_lba(cluster);
        
        // Read sectors for this cluster one by one
        for (uint32_t i = 0; i < g_fat_info.sectors_per_cluster && total_read < size; i++) {
            if (!ahci_read_sector(0, lba + i, (uint8_t*)buf + total_read)) {
                debugln("[fat32] Read failed at cluster %u, LBA %u", cluster, lba + i);
                return total_read; // Return what we got
            }
            total_read += 512;
        }

        cluster = get_fat_entry(cluster); // Get the next cluster in the chain
    }
    return total_read;
}

int fat32_vfs_readdir(vfs_node_t* node, uint32_t start_index, void* buf, size_t count) {
    uint32_t cluster = (uint32_t)node->data;
    void* dma_buffer = (void*)palloc_zero();
    uint8_t* sector_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_buffer);
    
    if (!disk_read_sector(g_fat_disk, cluster_to_lba(cluster), sector_buf)) {
        pfree(dma_buffer);
        return -1;
    }

    FAT32_DIR_ENTRY* entries = (FAT32_DIR_ENTRY*)sector_buf;
    znu_dirent_t* user_ents = (znu_dirent_t*)buf;
    int found_count = 0;
    int user_idx = 0;
    size_t max_ents = count / sizeof(znu_dirent_t);

    for (int i = 0; i < 16; i++) {
        // Skip empty/deleted/LongFileName entries
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5 || (entries[i].attr & 0x0F) == 0x0F) continue;

        // Skip entries until we reach the current file position
        if (found_count < start_index) {
            found_count++;
            continue;
        }

        if (user_idx < max_ents) {
            // Format the FAT name (e.g., "HELLO   " -> "HELLO")
            fat32_format_name((char*)entries[i].name, user_ents[user_idx].name);
            
            user_ents[user_idx].type = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
            user_ents[user_idx].size = entries[i].size;
            
            user_idx++;
        }
        found_count++;
    }

    pfree(dma_buffer);
    return user_idx * sizeof(znu_dirent_t);
}

vfs_ops_t fat32_ops = {
    .read = fat32_vfs_read,
    .write = NULL,
    .readdir = fat32_vfs_readdir,
    .find_node = fat32_vfs_find
};

