#ifndef _VFSE_H
#define _VFSE_H

#include <vfs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

struct vfse_stat {
    uint32_t st_dev;
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    uint64_t st_size;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
    uint32_t st_blksize;
    uint64_t st_blocks;
};

struct vfse_dirent {
    uint32_t  d_ino;
    uint32_t  d_off;
    uint16_t  d_reclen;
    uint8_t   d_type;
    char      d_name[256];
};

#define VFSE_O_RDONLY     0x0000
#define VFSE_O_WRONLY     0x0001
#define VFSE_O_RDWR       0x0002
#define VFSE_O_CREAT      0x0040
#define VFSE_O_EXCL       0x0080
#define VFSE_O_TRUNC      0x0200
#define VFSE_O_APPEND     0x0400
#define VFSE_O_NONBLOCK   0x0800
#define VFSE_O_DIRECTORY  0x10000

#define VFSE_SEEK_SET  0
#define VFSE_SEEK_CUR  1
#define VFSE_SEEK_END  2

#define VFSE_F_OK  0
#define VFSE_X_OK  1
#define VFSE_W_OK  2
#define VFSE_R_OK  4

#define VFSE_F_DUPFD    0
#define VFSE_F_GETFD    1
#define VFSE_F_SETFD    2
#define VFSE_F_GETFL    3
#define VFSE_F_SETFL    4

#define VFSE_S_IFMT     0xF000
#define VFSE_S_IFDIR    0x4000
#define VFSE_S_IFCHR    0x2000
#define VFSE_S_IFBLK    0x6000
#define VFSE_S_IFREG    0x8000
#define VFSE_S_IFIFO    0x1000
#define VFSE_S_IFLNK    0xA000
#define VFSE_S_IFSOCK   0xC000

#define VFSE_MS_RDONLY  1
#define VFSE_MS_NOSUID  2
#define VFSE_MS_NODEV   4
#define VFSE_MS_NOEXEC  8

#define VFSE_MAX_FDS    256

struct vfse_pipe {
    uint8_t* buffer;
    size_t size;
    size_t read_pos;
    size_t write_pos;
    bool read_closed;
    bool write_closed;
};

typedef struct vfse_file {
    vfs_node_t* node;
    size_t pos;
    int flags;
    int fd_flags;
    int ref_count;
    struct vfse_pipe* pipe;
    void* private;
} vfse_file_t;

typedef struct vfse_process {
    vfse_file_t* fds[VFSE_MAX_FDS];
    char cwd[4096];
    int next_fd;
} vfse_process_t;

extern vfse_process_t* vfse_current;

void vfse_init(void);

int vfse_open(const char* path, int flags, uint32_t mode);
int vfse_close(int fd);
ssize_t vfse_read(int fd, void* buf, size_t count);
ssize_t vfse_write(int fd, const void* buf, size_t count);
off_t vfse_lseek(int fd, off_t offset, int whence);
int vfse_dup(int fd);
int vfse_dup2(int oldfd, int newfd);
int vfse_fcntl(int fd, int cmd, long arg);

int vfse_stat(const char* path, struct vfse_stat* buf);
int vfse_lstat(const char* path, struct vfse_stat* buf);
int vfse_fstat(int fd, struct vfse_stat* buf);

int vfse_mkdir(const char* path, uint32_t mode);
int vfse_rmdir(const char* path);
int vfse_getdents(int fd, void* buf, size_t count);

int vfse_symlink(const char* target, const char* linkpath);
ssize_t vfse_readlink(const char* path, char* buf, size_t bufsize);
int vfse_unlink(const char* path);

int vfse_chdir(const char* path);
char* vfse_getcwd(char* buf, size_t size);

int vfse_access(const char* path, int mode);

int vfse_pipe(int pipefd[2]);

int vfse_mount(const char* source, const char* target,
               const char* fstype, unsigned long flags,
               const void* data);
int vfse_umount(const char* target);

char* vfse_resolve_path(const char* path, char* out, size_t out_len);

#endif
