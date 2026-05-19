#include <ff.h>
#include <vfs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syscall.h>
#include <page.h>
#include <disk.h>

static FATFS g_fatfs;
static bool g_mounted = false;

vfs_node_t* fat32_vfs_find(vfs_node_t* parent, const char* name);
int fat32_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset);
int fat32_vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
int fat32_vfs_readdir(vfs_node_t* node, uint32_t start_index, void* buf, size_t count);

vfs_ops_t fat32_ops = {
    .read = fat32_vfs_read,
    .write = fat32_vfs_write,
    .readdir = fat32_vfs_readdir,
    .find_node = fat32_vfs_find
};

bool fat32_init_on_disk(disk_t* d) {
    (void)d;
    
    FRESULT res = f_mount(&g_fatfs, "0:", 1);
    if (res != FR_OK) {
        debugln("[fat32] f_mount failed with error code: %d", res);
        return false;
    }
    
    g_mounted = true;
    return true;
}

vfs_node_t* fat32_vfs_find(vfs_node_t* parent, const char* name) {
    if (!g_mounted) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "0:/%s", name); 

    FILINFO fno;
    FRESULT res = f_stat(path, &fno);
    if (res != FR_OK) {
        return NULL; /* File or folder does not exist */
    }

    int type = (fno.fattrib & AM_DIR) ? VFS_DIRECTORY : VFS_FILE;
    vfs_node_t* node = vfs_create_node(name, type);
    if (!node) return NULL;

    node->size = fno.fsize;
    node->ops = &fat32_ops;
    
    if (type == VFS_FILE) {
        FIL* file_handle = kmalloc(sizeof(FIL));
        if (f_open(file_handle, path, FA_READ | FA_WRITE) == FR_OK) {
            node->data = (uintptr_t)file_handle;
        } else {
            kfree(file_handle);
            kfree(node);
            return NULL;
        }
    } else {
        DIR* dir_handle = kmalloc(sizeof(DIR));
        if (f_opendir(dir_handle, path) == FR_OK) {
            node->data = (uintptr_t)dir_handle;
        } else {
            kfree(dir_handle);
            kfree(node);
            return NULL;
        }
    }

    return node;
}

int fat32_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (node->type != VFS_FILE || !node->data) return -1;
    
    FIL* file = (FIL*)node->data;
    FRESULT res;

    /* Move file pointer if offset changes */
    if (f_tell(file) != offset) {
        res = f_lseek(file, (FSIZE_t)offset);
        if (res != FR_OK) return -1;
    }

    UINT bytes_read = 0;
    res = f_read(file, buf, (UINT)size, &bytes_read);
    if (res != FR_OK) return -1;

    return (int)bytes_read;
}

int fat32_vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    if (node->type != VFS_FILE || !node->data) return -1;

    FIL* file = (FIL*)node->data;
    FRESULT res;

    if (f_tell(file) != offset) {
        res = f_lseek(file, (FSIZE_t)offset);
        if (res != FR_OK) return -1;
    }

    UINT bytes_written = 0;
    res = f_write(file, buf, (UINT)size, &bytes_written);
    if (res != FR_OK) return -1;

    /* Auto synchronize layout allocations safely */
    f_sync(file);
    
    if (file->obj.objsize > node->size) {
        node->size = file->obj.objsize;
    }

    return (int)bytes_written;
}

int fat32_vfs_readdir(vfs_node_t* node, uint32_t start_index, void* buf, size_t count) {
    if (node->type != VFS_DIRECTORY || !node->data) return -1;

    DIR* dir = (DIR*)node->data;
    znu_dirent_t* user_ents = (znu_dirent_t*)buf;
    size_t max_ents = count / sizeof(znu_dirent_t);
    
    f_readdir(dir, NULL);
    
    uint32_t current_idx = 0;
    size_t user_idx = 0;
    FILINFO fno;

    while (user_idx < max_ents) {
        FRESULT res = f_readdir(dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break; /* End of directory loop */

        if (current_idx < start_index) {
            current_idx++;
            continue;
        }

        /* Map metadata components */
        strncpy(user_ents[user_idx].name, fno.fname, 127);
        user_ents[user_idx].type = (fno.fattrib & AM_DIR) ? VFS_DIRECTORY : VFS_FILE;
        user_ents[user_idx].size = fno.fsize;

        user_idx++;
        current_idx++;
    }

    return (int)(user_idx * sizeof(znu_dirent_t));
}
