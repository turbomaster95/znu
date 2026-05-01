#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>
#include <stdint.h>

typedef uint64_t dev_t;
typedef uint64_t ino_t;
typedef uint64_t nlink_t;

struct stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    long st_mtime;
        long st_atime;
        long st_ctime;
    };

    #define S_IFMT   0170000

#define S_IFDIR  0040000
#define S_IFREG  0100000

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)



int stat(const char* pathname, struct stat* statbuf);
int lstat(const char* pathname, struct stat* statbuf);
int stat64(const char* pathname, struct stat* statbuf);
int lstat64(const char* pathname, struct stat* statbuf);
int fstat(int fd, struct stat* statbuf);

#endif
