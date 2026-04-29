#ifndef _VFS_H
#define _VFS_H

#include <stddef.h>
#include <stdint.h>

#define VFS_FILE      1
#define VFS_DIRECTORY 2
#define VFS_DEVICE    3
#define VFS_SYMLINK   4

typedef struct vfs_node {
    char name[128];
    int type;
    int mode;
    int uid;
    int gid;
    uintptr_t data;
    size_t size;
    struct vfs_node* parent;
    struct vfs_node* children;
    struct vfs_node* next;
} vfs_node_t;

typedef struct vfs_file {
    vfs_node_t* node;
    size_t pos;
    int flags;
} vfs_file_t;

extern vfs_node_t* root_node;

// Core VFS functions
void init_vfs(void);
vfs_node_t* vfs_create_node(const char* name, int type);
void vfs_add_child(vfs_node_t* parent, vfs_node_t* child);
vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name);
vfs_node_t* vfs_path_to_node(const char* path);
void vfs_register_file(const char* path, uintptr_t data, size_t size);

// File access functions
int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset);
int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);

// CPIO
void cpio_parse(void *addr);

// Debug
void debug_vfs(vfs_node_t* node, int tab);

#endif
