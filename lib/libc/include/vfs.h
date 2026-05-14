#ifndef _VFS_H
#define _VFS_H

#include <stddef.h>
#include <stdint.h>

#define VFS_FILE      1
#define VFS_DIRECTORY 2
#define VFS_DEVICE    3
#define VFS_SYMLINK   4

// Forward declaration
struct vfs_node;
typedef struct vfs_node vfs_node_t;

typedef struct vfs_ops {
    int (*read)(struct vfs_node* node, void* buf, size_t size, size_t offset);
    int (*write)(struct vfs_node* node, const void* buf, size_t size, size_t offset);
    int (*readdir)(vfs_node_t* node, uint32_t index, void* buf, size_t count);
    struct vfs_node* (*find_node)(struct vfs_node* parent, const char* name);
} vfs_ops_t;

struct vfs_node {
    char name[128];
    int type;
    int mode;
    int uid;
    int gid;
    uintptr_t data;
    size_t size;

    vfs_ops_t* ops;          // Pointer to the driver functions
    int is_mountpoint;       // 1 if this node redirects to a disk driver
    void* internal_data;     // Stores filesystem-specific data (like FIL*)

    struct vfs_node* parent;
    struct vfs_node* children;
    struct vfs_node* next;
};


typedef struct vfs_file_instance {
    vfs_node_t* node;
    size_t pos;
    int flags;
    void* private;   // tty buffer, pipe state, etc.
} vfs_file_instance_t;

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
bool vfs_mount(const char* device, const char* fs_type, const char* path);

// File access functions
int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset);
int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);

// CPIO & Debug
void cpio_parse(void *addr);
void debug_vfs(vfs_node_t* node, int tab);

#endif
