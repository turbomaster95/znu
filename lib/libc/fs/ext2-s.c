#include <stdint.h>
#include <stddef.h>

// Forward declarations for kernel allocator
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);

/* --- AHCI Disk I/O Stubs --- */

int ahci_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    (void)lba; (void)count; (void)buf;
    return -1; // Return I/O error
}

int ahci_write_sectors(uint64_t lba, uint32_t count, const void *buf) {
    (void)lba; (void)count; (void)buf;
    return -1;
}

/* --- ext2fs Memory Allocation Stubs --- */

int ext2fs_get_mem(unsigned long size, void *ptr) {
    if (!ptr) return -1;
    void *p = kmalloc(size);
    *(void **)ptr = p;
    return p ? 0 : -1;
}

void ext2fs_free_mem(void *ptr) {
    if (!ptr) return;
    void **p = (void **)ptr;
    if (*p) {
        kfree(*p);
        *p = NULL;
    }
}

/* --- ext2fs Core Filesystem Stubs --- */

int ext2fs_open(const char *name, int flags, int superblock, int block_size, void *io_ptr, void **ret_fs) {
    (void)name; (void)flags; (void)superblock; (void)block_size; (void)io_ptr;
    if (ret_fs) *ret_fs = NULL;
    return -1;
}

int ext2fs_close(void *fs) {
    (void)fs;
    return 0;
}

int ext2fs_read_inode(void *fs, uint32_t ino, void *inode) {
    (void)fs; (void)ino; (void)inode;
    return -1;
}

int ext2fs_read_block_bitmap(void *fs) {
    (void)fs;
    return -1;
}

int ext2fs_read_inode_bitmap(void *fs) {
    (void)fs;
    return -1;
}

int ext2fs_namei(void *fs, uint32_t root, uint32_t cwd, const char *name, uint32_t *res_inode) {
    (void)fs; (void)root; (void)cwd; (void)name;
    if (res_inode) *res_inode = 0;
    return -1;
}

int ext2fs_dir_iterate(void *fs, uint32_t dir, int flags, void *block_buf,
                       int (*func)(uint32_t, int, void *, int, int, void *), void *priv_data) {
    (void)fs; (void)dir; (void)flags; (void)block_buf; (void)func; (void)priv_data;
    return -1;
}

/* --- ext2fs File Handle Stubs --- */

int ext2fs_file_open(void *fs, uint32_t ino, int flags, void **ret_file) {
    (void)fs; (void)ino; (void)flags;
    if (ret_file) *ret_file = NULL;
    return -1;
}

int ext2fs_file_close(void *file) {
    (void)file;
    return 0;
}

int ext2fs_file_read(void *file, void *buf, uint32_t wanted, uint32_t *got) {
    (void)file; (void)buf; (void)wanted;
    if (got) *got = 0;
    return -1;
}

int ext2fs_file_write(void *file, const void *buf, uint32_t nbytes, uint32_t *written) {
    (void)file; (void)buf; (void)nbytes;
    if (written) *written = 0;
    return -1;
}

int ext2fs_file_flush(void *file) {
    (void)file;
    return 0;
}

int ext2fs_file_llseek(void *file, uint64_t offset, int whence, uint64_t *new_pos) {
    (void)file; (void)offset; (void)whence;
    if (new_pos) *new_pos = 0;
    return -1;
}
