#ifndef _USERLIBC_SYS_STATFS_H
#define _USERLIBC_SYS_STATFS_H

#include <stdint.h>

// Unique identifier type for entire storage filesystems
typedef struct { 
    int val[2]; 
} fsid_t;

// The standard Linux structural layout BusyBox expects
struct statfs {
    long f_type;     // Magic signature identifying FS type (e.g., BTRFS_SUPER_MAGIC)
    long f_bsize;    // Optimal transfer block size 
    long f_blocks;   // Total data blocks in the filesystem
    long f_bfree;    // Free blocks available
    long f_bavail;   // Free blocks available to unprivileged users
    long f_files;    // Total file nodes (inodes) allocation capacity
    long f_ffree;    // Free file nodes available
    fsid_t f_fsid;   // Filesystem ID tag
    long f_namelen;  // Maximum length of filenames (typically 255)
    long f_frsize;   // Fragment size (often identical to block size)
    long f_flags;    // Mount flags configuration bitmask
    long f_spare[4]; // Padding reserved for future expansion
};

// Common filesystem magic type signatures checked by BusyBox tools
#define TMPFS_MAGIC          0x01021994
#define RAMFS_MAGIC          0x858458f6
#define MSDOS_SUPER_MAGIC    0x4d44
#define BTRFS_SUPER_MAGIC    0x9123683e

// Function prototypes for userlibc linking maps
int statfs(const char *path, struct statfs *buf);
int fstatfs(int fd, struct statfs *buf);

#endif // _USERLIBC_SYS_STATFS_H
