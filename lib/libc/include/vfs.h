#ifndef _VFS_H
#define _VFS_H

#include <stddef.h>
#include <stdint.h>

#define VFS_FILE      1
#define VFS_DIRECTORY 2
#define VFS_DEVICE    3

typedef struct vfs_node {
    char name[128];
    int type;
    uintptr_t data;
    size_t size;
    struct vfs_node* parent;
    struct vfs_node* children;
    struct vfs_node* next;
} vfs_node_t;

// FIX: extern declaration, define in ONE .c file
extern vfs_node_t* root_node;

vfs_node_t* vfs_create_node(const char* name, int type);
void vfs_add_child(vfs_node_t* parent, vfs_node_t* child);
vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name);
vfs_node_t* vfs_path_to_node(const char* path);
void init_vfs(void);
void cpio_parse(void *addr);
void vfs_register_file(const char* path, uintptr_t data, size_t size);
void debug_vfs(vfs_node_t* node, int tab);

#endif
