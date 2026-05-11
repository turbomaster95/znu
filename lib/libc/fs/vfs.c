#include <vfs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <page.h>
#include <devfs.h>

vfs_node_t* root_node = NULL;

int mem_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (!node->data || offset >= node->size) return 0;
    size_t to_read = (offset + size > node->size) ? (node->size - offset) : size;
    memcpy(buf, (void*)(node->data + offset), to_read);
    return (int)to_read;
}

vfs_ops_t mem_ops = {
    .read = mem_vfs_read,
    .write = NULL,
    .find_node = NULL
};

vfs_node_t* vfs_create_node(const char* name, int type) {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, 127);
    node->type = type;
    node->ops = &mem_ops; 
    return node;
}

void vfs_add_child(vfs_node_t* parent, vfs_node_t* child) {
    if (!parent || !child || parent->type != VFS_DIRECTORY) return;
    child->next = parent->children;
    parent->children = child;
    child->parent = parent;
}

vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name) {
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
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
            return curr->ops->find_node(curr, part);
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

        if (*ptr == '\0') { // Leaf node
            vfs_node_t* file = vfs_create_node(part, VFS_FILE);
            if (file) {
                file->data = data;
                file->size = size;
                vfs_add_child(curr, file);
            }
            break;
        } else { // Intermediate directory
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

void init_vfs() {
    root_node = vfs_create_node("/", VFS_DIRECTORY);
    devfs_init();
}

int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    if (!node || !node->ops || !node->ops->write) {
        // If the filesystem doesn't support writing (like CPIO), return an error
        return -1; 
    }
    return node->ops->write(node, buf, size, offset);
}
