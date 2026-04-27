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
    strncpy(node->name, name, 127); // Leave space for null terminator
    node->type = type;

    return node;
}

/**
 * vfs_add_child: Links a node into a directory's child list
 */
void vfs_add_child(vfs_node_t* parent, vfs_node_t* child) {
    if (!parent || !child || parent->type != VFS_DIRECTORY) return;

    child->next = parent->children;
    parent->children = child;
    child->parent = parent;
}

/**
 * vfs_find_child: Search for a specific entry name within a directory
 */
vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name) {
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;

    vfs_node_t* curr = parent->children;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * vfs_ensure_dir: Get or create a directory node
 */
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

/**
 * vfs_register_file: Maps a normalized path (e.g. "/bin/init") into the VFS tree.
 * Creates parent directories as needed.
 */
void vfs_register_file(const char* path, uintptr_t data, size_t size) {
    if (!root_node || !path || path[0] != '/') return;

    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    vfs_node_t* curr = root_node;
    char* p = path_copy + 1; // skip leading '/'
    char* component = p;

    while (1) {
        // Find end of this component
        while (*p && *p != '/') p++;
        int is_last = (*p == '\0');
        char saved = *p;
        *p = '\0';

        if (strlen(component) == 0) {
            // empty component (e.g. "//" or trailing "/")
            if (is_last) break;
            *p = saved;
            p++;
            component = p;
            continue;
        }

        if (is_last) {
            // Last component: create file
            vfs_node_t* file = vfs_create_node(component, VFS_FILE);
            if (file) {
                file->data = data;
                file->size = size;
                vfs_add_child(curr, file);
            }
            break;
        } else {
            // Intermediate component: must be directory
            vfs_node_t* next = vfs_ensure_dir(curr, component);
            if (!next) return; // failed
            curr = next;
        }

        *p = saved;
        p++;
        component = p;
    }
}

vfs_node_t* vfs_path_to_node(const char* path) {
    if (path == NULL || path[0] != '/') return NULL;
    if (path[1] == '\0') return root_node;

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
