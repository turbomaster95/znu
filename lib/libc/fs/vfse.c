#include <vfse.h>
#include <vfs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <page.h>

/* vfs extended (VFSe) to make linux programs think they are on linux */
 
vfse_process_t* vfse_current = NULL;

static vfse_file_t* vfse_alloc_file(void) {
    vfse_file_t* f = kmalloc(sizeof(vfse_file_t));
    if (f) memset(f, 0, sizeof(vfse_file_t));
    return f;
}

static void vfse_free_file(vfse_file_t* f) {
    if (!f) return;
    if (f->pipe) {
        if (f->pipe->buffer) kfree(f->pipe->buffer);
        kfree(f->pipe);
    }
    kfree(f);
}

static int vfse_alloc_fd(vfse_file_t* file) {
    if (!vfse_current) return -1;
    for (int i = vfse_current->next_fd; i < VFSE_MAX_FDS; i++) {
        if (!vfse_current->fds[i]) {
            vfse_current->fds[i] = file;
            vfse_current->next_fd = i + 1;
            return i;
        }
    }
    for (int i = 3; i < vfse_current->next_fd; i++) {
        if (!vfse_current->fds[i]) {
            vfse_current->fds[i] = file;
            vfse_current->next_fd = i + 1;
            return i;
        }
    }
    return -1;
}

static void vfse_free_fd(int fd) {
    if (fd < 0 || fd >= VFSE_MAX_FDS) return;
    if (!vfse_current) return;
    vfse_file_t* f = vfse_current->fds[fd];
    if (!f) return;
    f->ref_count--;
    if (f->ref_count <= 0) {
        vfse_free_file(f);
    }
    vfse_current->fds[fd] = NULL;
    if (fd < vfse_current->next_fd) {
        vfse_current->next_fd = fd;
    }
}

static vfse_file_t* vfse_get_file(int fd) {
    if (fd < 0 || fd >= VFSE_MAX_FDS) return NULL;
    if (!vfse_current) return NULL;
    return vfse_current->fds[fd];
}

char* vfse_resolve_path(const char* path, char* out, size_t out_len) {
    if (!path || !out || out_len < 2) return NULL;

    if (path[0] == '/') {
        strncpy(out, path, out_len - 1);
        out[out_len - 1] = '\0';
        return out;
    }

    if (!vfse_current || vfse_current->cwd[0] == '\0') {
        out[0] = '/';
        out[1] = '\0';
    } else {
        size_t cwd_len = strlen(vfse_current->cwd);
        if (cwd_len >= out_len - 1) return NULL;
        strcpy(out, vfse_current->cwd);
        if (out[cwd_len - 1] != '/') {
            out[cwd_len] = '/';
            out[cwd_len + 1] = '\0';
            cwd_len++;
        }
        strncpy(out + cwd_len, path, out_len - cwd_len - 1);
        out[out_len - 1] = '\0';
    }
    return out;
}

static void vfse_fill_stat(vfs_node_t* node, struct vfse_stat* st) {
    memset(st, 0, sizeof(struct vfse_stat));
    if (!node) return;

    st->st_size = node->size;
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_nlink = 1;
    st->st_ino = (uint32_t)(uintptr_t)node;
    st->st_dev = 0;
    st->st_blksize = 4096;
    st->st_blocks = (node->size + 511) / 512;

    if (node->type == VFS_DIRECTORY) {
        st->st_mode = VFSE_S_IFDIR | (node->mode & 0777);
    } else if (node->type == VFS_FILE) {
        st->st_mode = VFSE_S_IFREG | (node->mode & 0777);
    } else if (node->type == VFS_DEVICE) {
        st->st_mode = VFSE_S_IFCHR | (node->mode & 0777);
    } else if (node->type == VFS_SYMLINK) {
        st->st_mode = VFSE_S_IFLNK | (node->mode & 0777);
    }
}

void vfse_init(void) {
    vfse_current = kmalloc(sizeof(vfse_process_t));
    if (!vfse_current) return;
    memset(vfse_current, 0, sizeof(vfse_process_t));
    vfse_current->cwd[0] = '/';
    vfse_current->cwd[1] = '\0';
    vfse_current->next_fd = 3;
}

int vfse_open(const char* path, int flags, uint32_t mode) {
    (void)mode;
    if (!path) return -1;

    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) {
        if (!(flags & VFSE_O_CREAT)) return -1;
        return -1;
    }

    if ((flags & VFSE_O_DIRECTORY) && node->type != VFS_DIRECTORY) {
        return -1;
    }

    vfse_file_t* file = vfse_alloc_file();
    if (!file) return -1;

    file->node = node;
    file->pos = 0;
    file->flags = flags;
    file->ref_count = 1;

    if (flags & VFSE_O_APPEND) {
        file->pos = node->size;
    }

    int fd = vfse_alloc_fd(file);
    if (fd < 0) {
        vfse_free_file(file);
        return -1;
    }
    return fd;
}

int vfse_close(int fd) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file) return -1;
    vfse_free_fd(fd);
    return 0;
}

ssize_t vfse_read(int fd, void* buf, size_t count) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file) return -1;
    if (!buf) return -1;

    if (file->pipe) {
        if (file->pipe->read_closed) return 0;
        size_t n = 0;
        while (n < count && file->pipe->read_pos != file->pipe->write_pos) {
            ((uint8_t*)buf)[n++] = file->pipe->buffer[file->pipe->read_pos++];
            if (file->pipe->read_pos >= file->pipe->size)
                file->pipe->read_pos = 0;
        }
        return (ssize_t)n;
    }

    if (!file->node || !file->node->ops || !file->node->ops->read) {
        return -1;
    }

    int ret = file->node->ops->read(file->node, buf, count, file->pos);
    if (ret > 0) file->pos += ret;
    return (ssize_t)ret;
}

ssize_t vfse_write(int fd, const void* buf, size_t count) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file) return -1;
    if (!buf) return -1;

    if (file->pipe) {
        if (file->pipe->write_closed) return -1;
        size_t n = 0;
        while (n < count) {
            size_t next = (file->pipe->write_pos + 1) % file->pipe->size;
            if (next == file->pipe->read_pos) break;
            file->pipe->buffer[file->pipe->write_pos] = ((uint8_t*)buf)[n++];
            file->pipe->write_pos = next;
        }
        return (ssize_t)n;
    }

    if (!file->node || !file->node->ops || !file->node->ops->write) {
        return -1;
    }

    if (!(file->flags & VFSE_O_WRONLY) && !(file->flags & VFSE_O_RDWR)) {
        return -1;
    }

    int ret = file->node->ops->write(file->node, buf, count, file->pos);
    if (ret > 0) {
        file->pos += ret;
        if (file->pos > file->node->size) file->node->size = file->pos;
    }
    return (ssize_t)ret;
}

off_t vfse_lseek(int fd, off_t offset, int whence) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file || !file->node) return -1;
    if (file->pipe) return -1;

    off_t new_pos;
    switch (whence) {
        case VFSE_SEEK_SET: new_pos = offset; break;
        case VFSE_SEEK_CUR: new_pos = (off_t)file->pos + offset; break;
        case VFSE_SEEK_END: new_pos = (off_t)file->node->size + offset; break;
        default: return -1;
    }

    if (new_pos < 0) return -1;
    file->pos = (size_t)new_pos;
    return new_pos;
}

int vfse_dup(int fd) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file) return -1;
    file->ref_count++;
    int newfd = vfse_alloc_fd(file);
    if (newfd < 0) {
        file->ref_count--;
        return -1;
    }
    return newfd;
}

int vfse_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= VFSE_MAX_FDS) return -1;
    if (newfd < 0 || newfd >= VFSE_MAX_FDS) return -1;
    if (!vfse_current) return -1;

    vfse_file_t* file = vfse_current->fds[oldfd];
    if (!file) return -1;

    if (vfse_current->fds[newfd]) {
        vfse_close(newfd);
    }

    file->ref_count++;
    vfse_current->fds[newfd] = file;
    return newfd;
}

int vfse_fcntl(int fd, int cmd, long arg) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file) return -1;

    switch (cmd) {
        case VFSE_F_DUPFD: {
            int minfd = (int)arg;
            if (minfd < 0) minfd = 0;
            file->ref_count++;
            for (int i = minfd; i < VFSE_MAX_FDS; i++) {
                if (!vfse_current->fds[i]) {
                    vfse_current->fds[i] = file;
                    return i;
                }
            }
            file->ref_count--;
            return -1;
        }
        case VFSE_F_GETFD:
            return file->fd_flags;
        case VFSE_F_SETFD:
            file->fd_flags = (int)arg;
            return 0;
        case VFSE_F_GETFL:
            return file->flags;
        case VFSE_F_SETFL:
            file->flags = (int)arg;
            return 0;
    }
    return -1;
}

int vfse_stat(const char* path, struct vfse_stat* buf) {
    if (!path || !buf) return -1;

    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) return -1;

    vfse_fill_stat(node, buf);
    return 0;
}

int vfse_lstat(const char* path, struct vfse_stat* buf) {
    return vfse_stat(path, buf);
}

int vfse_fstat(int fd, struct vfse_stat* buf) {
    if (!buf) return -1;
    vfse_file_t* file = vfse_get_file(fd);
    if (!file || !file->node) return -1;

    vfse_fill_stat(file->node, buf);
    return 0;
}

int vfse_getdents(int fd, void* buf, size_t count) {
    vfse_file_t* file = vfse_get_file(fd);
    if (!file || !file->node) return -1;
    if (file->node->type != VFS_DIRECTORY) return -1;

    uint8_t* ptr = (uint8_t*)buf;
    int total = 0;

    /* mountpoint - delegate to fs driver */
    if (file->node->is_mountpoint && file->node->ops && file->node->ops->readdir) {
        return file->node->ops->readdir(file->node, file->pos, buf, count);
    }

    /* memfs - walk children */
    vfs_node_t* child = file->node->children;
    uint32_t idx = 0;
    while (child && idx < file->pos) {
        child = child->next;
        idx++;
    }

    while (child && (total + sizeof(struct vfse_dirent)) <= count) {
        struct vfse_dirent dent;
        memset(&dent, 0, sizeof(dent));

        dent.d_ino = (uint32_t)(uintptr_t)child;
        dent.d_off = idx + 1;
        dent.d_reclen = sizeof(struct vfse_dirent);

        switch (child->type) {
            case VFS_DIRECTORY: dent.d_type = 4; break;
            case VFS_FILE:        dent.d_type = 8; break;
            case VFS_DEVICE:      dent.d_type = 2; break;
            case VFS_SYMLINK:     dent.d_type = 10; break;
            default:              dent.d_type = 0; break;
        }

        strncpy(dent.d_name, child->name, sizeof(dent.d_name) - 1);
        dent.d_name[sizeof(dent.d_name) - 1] = '\0';

        memcpy(ptr + total, &dent, sizeof(dent));
        total += sizeof(dent);

        child = child->next;
        idx++;
        file->pos++;
    }

    return total;
}

int vfse_mkdir(const char* path, uint32_t mode) {
    (void)mode;
    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;
    if (vfs_path_to_node(abspath)) return -1;
    return -1;
}

int vfse_rmdir(const char* path) {
    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) return -1;
    if (node->type != VFS_DIRECTORY) return -1;
    if (node->children) return -1;
    return -1;
}

int vfse_unlink(const char* path) {
    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) return -1;
    if (node->type == VFS_DIRECTORY) return -1;
    return -1;
}

int vfse_symlink(const char* target, const char* linkpath) {
    if (!target || !linkpath) return -1;

    char abspath[4096];
    if (!vfse_resolve_path(linkpath, abspath, sizeof(abspath))) return -1;

    char parent_path[4096];
    char* last_slash = strrchr(abspath, '/');
    if (!last_slash) return -1;
    
    size_t parent_len = last_slash - abspath;
    if (parent_len == 0) parent_len = 1;
    
    strncpy(parent_path, abspath, parent_len);
    parent_path[parent_len] = '\0';
    
    vfs_node_t* parent = vfs_path_to_node(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY) return -1;

    vfs_node_t* node = vfs_create_node(last_slash + 1, VFS_SYMLINK);
    if (!node) return -1;

    vfs_add_child(parent, node);

    node->type = VFS_SYMLINK;
    node->link_target = strdup(target); 

    return 0;
}

ssize_t vfse_readlink(const char* path, char* buf, size_t bufsize) {
    if (!path || !buf || !bufsize) return -1;
    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) return -1;
    if (node->type != VFS_SYMLINK) return -1;
    return -1;
}

int vfse_chdir(const char* path) {
    if (!path) return -1;
    if (!vfse_current) return -1;

    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) return -1;
    if (node->type != VFS_DIRECTORY) return -1;

    strncpy(vfse_current->cwd, abspath, sizeof(vfse_current->cwd) - 1);
    vfse_current->cwd[sizeof(vfse_current->cwd) - 1] = '\0';
    return 0;
}

char* vfse_getcwd(char* buf, size_t size) {
    if (!buf || size == 0) return NULL;
    if (!vfse_current) {
        buf[0] = '\0';
        return buf;
    }
    strncpy(buf, vfse_current->cwd, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

int vfse_access(const char* path, int mode) {
    char abspath[4096];
    if (!vfse_resolve_path(path, abspath, sizeof(abspath))) return -1;

    vfs_node_t* node = vfs_path_to_node(abspath);
    if (!node) return -1;

    (void)mode;
    return 0;
}

int vfse_pipe(int pipefd[2]) {
    if (!pipefd) return -1;

    struct vfse_pipe* p = kmalloc(sizeof(struct vfse_pipe));
    if (!p) return -1;
    p->size = 4096;
    p->buffer = kmalloc(p->size);
    if (!p->buffer) {
        kfree(p);
        return -1;
    }
    p->read_pos = 0;
    p->write_pos = 0;
    p->read_closed = false;
    p->write_closed = false;

    vfse_file_t* reader = vfse_alloc_file();
    vfse_file_t* writer = vfse_alloc_file();
    if (!reader || !writer) {
        if (reader) vfse_free_file(reader);
        if (writer) vfse_free_file(writer);
        kfree(p->buffer);
        kfree(p);
        return -1;
    }

    reader->pipe = p;
    writer->pipe = p;
    reader->ref_count = 1;
    writer->ref_count = 1;
    reader->flags = VFSE_O_RDONLY;
    writer->flags = VFSE_O_WRONLY;

    pipefd[0] = vfse_alloc_fd(reader);
    pipefd[1] = vfse_alloc_fd(writer);

    if (pipefd[0] < 0 || pipefd[1] < 0) {
        if (pipefd[0] >= 0) vfse_close(pipefd[0]);
        if (pipefd[1] >= 0) vfse_close(pipefd[1]);
        return -1;
    }
    return 0;
}

int vfse_mount(const char* source, const char* target,
               const char* fstype, unsigned long flags,
               const void* data) {
    (void)flags; (void)data;
    if (!source || !target || !fstype) return -1;

    char abspath[4096];
    if (!vfse_resolve_path(target, abspath, sizeof(abspath))) return -1;

    if (vfs_mount(source, fstype, abspath)) return 0;
    return -1;
}

int vfse_umount(const char* target) {
    if (!target) return -1;
    (void)target;
    return -1;
}
