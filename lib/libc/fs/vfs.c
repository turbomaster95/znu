#include <ff.h>
#include <disk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <page.h>
#include <devfs.h>
#include <fat32.h>
#include <vfs.h>
#include <errno.h>
#include <input.h>

vfs_node_t* root_node = NULL;

extern uint32_t g_root_cluster;
extern vfs_ops_t fat32_ops;

int mem_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (!node || !buf) return -1;
    if (!node->data || offset >= node->size) return 0;
    size_t to_read = (offset + size > node->size) ? (node->size - offset) : size;
    memcpy(buf, (void*)(node->data + offset), to_read);
    return (int)to_read;
}

int mem_vfs_ioctl(vfs_node_t* node, unsigned long request, void* argp) {
    (void)node; (void)request; (void)argp;
    return -ENOTTY;
}

int mem_vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    if (!node || !buf) return -1;

    size_t required_size = offset + size;

    if (required_size > node->size) {
        // Automatically grow the allocation buffer on the heap
        void* new_data = krealloc((void*)node->data, required_size);
        if (!new_data && required_size > 0) {
            return -ENOMEM;
        }
        node->data = (uintptr_t)new_data;
        node->size = required_size;
    }

    memcpy((void*)(node->data + offset), buf, size);
    return (int)size;
}

vfs_ops_t mem_ops;

struct vfs_node* mem_vfs_create(struct vfs_node* parent, const char* name, int type) {
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;

    vfs_node_t* new_node = vfs_create_node(name, type);
    if (!new_node) return NULL;

    new_node->ops = &mem_ops;

    vfs_add_child(parent, new_node);

    return new_node;
}

vfs_ops_t mem_ops = {
    .read      = mem_vfs_read,
    .write     = mem_vfs_write,
    .create    = mem_vfs_create,
    .readdir   = NULL,        /* getdents walks children directly for memfs */
    .find_node = NULL,
    .ioctl     = mem_vfs_ioctl
};

vfs_node_t* vfs_create_node(const char* name, int type) {
    if (!name || !name[0]) {
        debugln("[vfs] ERROR: vfs_create_node called with empty name!");
        return NULL;
    }
    
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, 127);
    node->name[127] = '\0';
    node->type = type;
    node->ops = &mem_ops;
    return node;
}

void vfs_add_child(vfs_node_t* parent, vfs_node_t* child) {
    if (!parent || !child || parent->type != VFS_DIRECTORY) return;
    if (child->parent) return;   /* Already attached somewhere */
    
    child->next = parent->children;
    parent->children = child;
    child->parent = parent;
}

void vfs_remove_child(vfs_node_t* parent, vfs_node_t* child) {
    if (!parent || !child || parent->type != VFS_DIRECTORY) return;
    
    vfs_node_t** curr = &parent->children;
    while (*curr) {
        if (*curr == child) {
            *curr = child->next;
            child->parent = NULL;
            child->next = NULL;
            return;
        }
        curr = &(*curr)->next;
    }
}

vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name) {
    if (!parent || !name || parent->type != VFS_DIRECTORY) return NULL;
    vfs_node_t* curr = parent->children;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

vfs_node_t* vfs_path_to_node(const char* path) {
    if (!path || path[0] != '/') return NULL;
    if (path[1] == '\0') return root_node;

    vfs_node_t* curr = root_node;
    const char* ptr = path + 1;

    while (*ptr) {
        while (*ptr == '/') ptr++; 
        if (*ptr == '\0') break;

        const char* start = ptr;
        while (*ptr && *ptr != '/') ptr++;
        size_t len = ptr - start;

        char part[128];
        if (len > 127) len = 127;
        memcpy(part, start, len);
        part[len] = '\0';

        if (curr->is_mountpoint && curr->ops && curr->ops->find_node) {
            vfs_node_t* disk_node = curr->ops->find_node(curr, part);
            if (disk_node) {
                vfs_add_child(curr, disk_node);
                curr = disk_node;
                continue; 
            }
        }

        vfs_node_t* next = vfs_find_child(curr, part);
        if (!next) return NULL;
        curr = next;
    }
    return curr;
}

void vfs_register_file(const char* path, uintptr_t data, size_t size) {
    if (!root_node || !path || path[0] != '/') return;

    vfs_node_t* curr = root_node;
    const char* ptr = path + 1;

    while (*ptr) {
        while (*ptr == '/') ptr++;
        if (*ptr == '\0') break;

        const char* start = ptr;
        while (*ptr && *ptr != '/') ptr++;
        size_t len = ptr - start;

        char part[128];
        if (len > 127) len = 127;
        memcpy(part, start, len);
        part[len] = '\0';

        /* Skip empty components (e.g. from trailing slash or double slash) */
        if (part[0] == '\0') {
            if (*ptr == '\0') break;
            continue;
        }

        if (*ptr == '\0') { /* Leaf node */
            vfs_node_t* file = vfs_create_node(part, VFS_FILE);
            if (file) {
                file->data = data;
                file->size = size;
                vfs_add_child(curr, file);
            }
            break;
        } else { /* Intermediate directory */
            vfs_node_t* dir = vfs_find_child(curr, part);
            if (!dir) {
                dir = vfs_create_node(part, VFS_DIRECTORY);
                vfs_add_child(curr, dir);
            }
            curr = dir;
        }
    }
}

int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (!node || !node->ops || !node->ops->read) return -1;
    return node->ops->read(node, buf, size, offset);
}

int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    if (!node || !node->ops || !node->ops->write) return -1;
    return node->ops->write(node, buf, size, offset);
}

int vfs_ioctl(vfs_node_t* node, unsigned long request, void* argp) {
    if (!node) return -1;
    if (!node->ops || !node->ops->ioctl) return -ENOTTY;
    return node->ops->ioctl(node, request, argp);
}

void init_vfs(void) {
    root_node = vfs_create_node("/", VFS_DIRECTORY);
    devfs_init();
    evdev_init();
    keyboard_init_evdev();
}

bool vfs_mount(const char* device, const char* fs_type, const char* path) {
    if (!device || !fs_type || !path) return false;

    debugln("[vfs] mount request: %s -> %s at %s", device, fs_type, path);

    if (strcmp(fs_type, "fat32") == 0) {
        disk_t* d = disk_get_by_name(device);
        if (!d) return false;

        if (!fat32_init_on_disk(d)) return false;

        /* Parse mount path instead of hardcoding "mnt" */
        vfs_node_t* parent = root_node;
        const char* basename = path;
        
        if (strcmp(path, "/") != 0) {
            /* Find parent directory */
            char* path_copy = strdup(path);
            if (!path_copy) return false;
            
            char* last_slash = strrchr(path_copy, '/');
            if (last_slash && last_slash != path_copy) {
                *last_slash = '\0';
                basename = last_slash + 1;
                parent = vfs_path_to_node(path_copy);
            } else if (last_slash == path_copy) {
                basename = path + 1;
            }
            kfree(path_copy);
            
            if (!parent) return false;
        }
        
        if (parent->type != VFS_DIRECTORY) return false;

        vfs_node_t* mount_node = vfs_create_node(basename, VFS_DIRECTORY);
        if (!mount_node) return false;

        mount_node->is_mountpoint = 1;
        mount_node->ops = &fat32_ops;

        DIR* root_dir = (DIR*)kmalloc(sizeof(DIR));
        if (root_dir && f_opendir(root_dir, "0:/") == FR_OK) {
            mount_node->internal_data = root_dir;   /* Use internal_data, not data */
        } else {
            debugln("[vfs] Failed to open FatFs root directory '0:/'");
            if (root_dir) kfree(root_dir);
            kfree(mount_node);
            return false;
        }

        vfs_add_child(parent, mount_node);
        debugln("[vfs] fat32 mounted at %s", path);
        return true;
    }

    debugln("[vfs] unknown filesystem: %s", fs_type);
    return false;
}

void debug_vfs(vfs_node_t* node, int tab) {
    if (!node) return;
    
    for (int i = 0; i < tab; i++) debug_write("  ");
    
    const char* type_str = "???";
    switch (node->type) {
        case VFS_FILE:      type_str = "FILE"; break;
        case VFS_DIRECTORY: type_str = "DIR "; break;
        case VFS_DEVICE:    type_str = "DEV "; break;
        case VFS_SYMLINK:   type_str = "LINK"; break;
    }
    
    debugln("[%s] \"%s\" size=%zu", type_str, node->name, node->size);
    
    vfs_node_t* child = node->children;
    while (child) {
        debug_vfs(child, tab + 1);
        child = child->next;
    }
}

vfs_file_t* dup_file(vfs_file_t* src) {
    if (!src) return NULL;
    
    vfs_file_t* new_file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    if (!new_file) return NULL;
    
    memcpy(new_file, src, sizeof(vfs_file_t));
    return new_file;
}
