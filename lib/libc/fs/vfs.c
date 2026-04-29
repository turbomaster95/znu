#include <vfs.h>
#include <stdlib.h>
#include <string.h>
#include <page.h>
#include <stdio.h>

vfs_node_t* root_node = NULL;

/**
 * vfs_create_node: Internal helper to allocate and zero a new node
 */
vfs_node_t* vfs_create_node(const char* name, int type) {
    vfs_node_t* node = kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, 127);
    node->type = type;
    node->mode = 0644; // Default permissions
    if (type == VFS_DIRECTORY) node->mode = 0755;

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

static vfs_node_t* vfs_ensure_dir(vfs_node_t* parent, const char* name) {
    vfs_node_t* node = vfs_find_child(parent, name);
    if (node) {
        if (node->type != VFS_DIRECTORY) return NULL; // name conflict
        return node;
    }
    node = vfs_create_node(name, VFS_DIRECTORY);
    if (!node) return NULL;
    vfs_add_child(parent, node);
    return node;
}

void vfs_register_file(const char* path, uintptr_t data, size_t size) {
    if (!root_node || !path || path[0] != '/') return;

    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    vfs_node_t* curr = root_node;
    char* p = path_copy + 1; 
    char* component = p;

    while (1) {
        while (*p && *p != '/') p++;
        int is_last = (*p == '\0');
        char saved = *p;
        *p = '\0';

        if (strlen(component) == 0) {
            if (is_last) break;
            *p = saved; p++; component = p;
            continue;
        }

        if (is_last) {
            vfs_node_t* existing = vfs_find_child(curr, component);
            if (existing) {
                existing->data = data;
                existing->size = size;
            } else {
                vfs_node_t* file = vfs_create_node(component, VFS_FILE);
                if (file) {
                    file->data = data;
                    file->size = size;
                    vfs_add_child(curr, file);
                }
            }
            break;
        } else {
            vfs_node_t* next = vfs_ensure_dir(curr, component);
            if (!next) return;
            curr = next;
        }

        *p = saved; p++; component = p;
    }
}

int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (!node || node->type != VFS_FILE) return -1;
    if (offset >= node->size) return 0;
    
    size_t to_read = size;
    if (offset + to_read > node->size) {
        to_read = node->size - offset;
    }
    
    if (!node->data) return -1;
    memcpy(buf, (void*)(node->data + offset), to_read);
    return to_read;
}

int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    // Basic memory-backed VFS doesn't support writing to fixed data pointers easily
    // unless we allocate new memory. For now, just a placeholder.
    if (!node || node->type != VFS_FILE) return -1;
    return -1; 
}

vfs_node_t* vfs_path_to_node(const char* path) {
    if (path == NULL || path[0] != '/') return NULL;
    
    // Handle "/" or any number of leading slashes
    const char* p = path;
    while (*p == '/') p++;
    if (*p == '\0') return root_node;

    vfs_node_t* curr = root_node;
    const char* ptr = path + 1; // skip leading '/'

    while (*ptr != '\0') {
        // Skip leading slashes
        while (*ptr == '/') ptr++;
        if (*ptr == '\0') break;

        const char* start = ptr;
        while (*ptr != '/' && *ptr != '\0') ptr++;
        size_t len = ptr - start;

        if (len == 0) continue;

        vfs_node_t* found = NULL;
        vfs_node_t* child = curr->children;

        while (child) {
            if (strncmp(child->name, start, len) == 0 && strlen(child->name) == len) {
                found = child;
                break;
            }
            child = child->next;
        }

        if (!found) return NULL;
        curr = found;
    }
    return curr;
}

/**
 * init_vfs: Entry point to boot the filesystem
 */
void init_vfs() {
    debugln("[VFS] Initializing Virtual File System...");
    root_node = vfs_create_node("/", VFS_DIRECTORY);
}

void debug_vfs(vfs_node_t* node, int tab) {
    if (!node) return;
    for(int i=0; i<tab; i++) printf("  ");
    printf("- %s (%s)\n", node->name, node->type == VFS_DIRECTORY ? "DIR" : "FILE");

    if (node->type == VFS_DIRECTORY) {
        vfs_node_t* child = node->children;
        while (child) {
            debug_vfs(child, tab + 1);
            child = child->next;
        }
    }
}
