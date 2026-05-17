#ifndef _USERLIBC_MNTENT_H
#define _USERLIBC_MNTENT_H

#include <stdio.h>

// Path to standard system mount description tables
#define MNTTAB      "/etc/fstab"  // Static table of filesystems
#define MOUNTED     "/etc/mtab"   // Table of currently mounted filesystems

// Structure describing an entry in a mount table list
struct mntent {
    char *mnt_fsname; // Name of mounted filesystem (e.g., "/dev/sda1")
    char *mnt_dir;    // Filesystem mount point path (e.g., "/")
    char *mnt_type;   // Type of filesystem (e.g., "vfat", "btrfs")
    char *mnt_opts;   // Mount options separated by commas (e.g., "rw,nosuid")
    int   mnt_freq;   // Dump frequency in days
    int   mnt_passno; // Pass number on parallel fsck check loops
};

// Core POSIX-adjacent function prototypes for mount tracking
FILE          *setmntent(const char *filename, const char *type);
struct mntent *getmntent(FILE *stream);
int            addmntent(FILE *stream, const struct mntent *mnt);
int            endmntent(FILE *stream);
char          *hasmntopt(const struct mntent *mnt, const char *opt);

#endif // _USERLIBC_MNTENT_H
