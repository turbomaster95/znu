#define FILE void

#include <stdio.h>
#include "ext2fs/ext2_types.h"
#include <ext2fs/ext2fs.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern void debugln(const char* fmt, ...);

extern int ahci_read_sectors(uint64_t lba, uint32_t count, void* buffer);
extern int ahci_write_sectors(uint64_t lba, uint32_t count, const void* buffer);

#define VFS_FILE      1
#define VFS_DIRECTORY 2

typedef struct vfs_node {
    char name[128];
    int type;
    size_t size;
    struct vfs_ops* ops;
    uintptr_t data;
} vfs_node_t;

typedef struct vfs_ops {
    int (*read)(vfs_node_t* node, void* buf, size_t size, size_t offset);
    int (*write)(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    int (*readdir)(vfs_node_t* node, uint32_t start_index, void* buf, size_t count);
    vfs_node_t* (*find_node)(vfs_node_t* parent, const char* name);
} vfs_ops_t;

typedef struct {
    char name[128];
    int type;
    size_t size;
} znu_dirent_t;

extern vfs_node_t* vfs_create_node(const char* name, int type);

struct znu_ext2_io_private {
    uint64_t base_lba;
    uint32_t block_size;
    uint32_t sectors_per_block;
};

static errcode_t znu_ext2_io_open(const char* name, int flags, io_channel* channel)
{
    struct znu_ext2_io_private* priv;
    io_channel ch;
    errcode_t retval;

    (void)name;
    (void)flags;

    retval = ext2fs_get_mem(sizeof(struct struct_io_channel), (void**)&ch);
    if (retval != 0) {
        debugln("[ext2] io_open: failed to allocate io_channel memory");
        return retval;
    }

    memset(ch, 0, sizeof(struct struct_io_channel));

    retval = ext2fs_get_mem(sizeof(struct znu_ext2_io_private), (void**)&priv);
    if (retval != 0) {
        ext2fs_free_mem((void**)&ch);
        debugln("[ext2] io_open: failed to allocate private data");
        return retval;
    }

    memset(priv, 0, sizeof(struct znu_ext2_io_private));
    priv->base_lba = 0;
    priv->block_size = 1024;
    priv->sectors_per_block = 2;

    ch->magic = EXT2_ET_MAGIC_IO_CHANNEL;
    ch->block_size = 1024;
    ch->read_error = 0;
    ch->write_error = 0;
    ch->refcount = 1;
    ch->private_data = priv;
    ch->manager = NULL;

    *channel = ch;
    debugln("[ext2] io_open: channel initialized");
    return 0;
}

static errcode_t znu_ext2_io_close(io_channel channel)
{
    struct znu_ext2_io_private* priv;
    errcode_t retval = 0;

    if (channel == NULL) {
        return EXT2_ET_BAD_DEVICE_NAME;
    }

    if (channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    priv = (struct znu_ext2_io_private*)channel->private_data;
    if (priv != NULL) {
        ext2fs_free_mem((void**)&priv);
    }

    channel->magic = 0;
    ext2fs_free_mem((void**)&channel);

    debugln("[ext2] io_close: channel closed");
    return retval;
}

static errcode_t znu_ext2_io_set_blksize(io_channel channel, int blksize)
{
    struct znu_ext2_io_private* priv;

    if (channel == NULL || channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    priv = (struct znu_ext2_io_private*)channel->private_data;
    if (priv == NULL) {
        return EXT2_ET_BAD_DEVICE_NAME;
    }

    channel->block_size = blksize;
    priv->block_size = (uint32_t)blksize;
    priv->sectors_per_block = (uint32_t)(blksize / 512);

    if (priv->sectors_per_block == 0) {
        priv->sectors_per_block = 1;
    }

    debugln("[ext2] set_blksize: block_size=%d, sectors_per_block=%d",
            blksize, priv->sectors_per_block);
    return 0;
}

static errcode_t znu_ext2_io_read_blk(io_channel channel,
                                       unsigned long block,
                                       int count,
                                       void* buf)
{
    struct znu_ext2_io_private* priv;
    uint64_t lba;
    uint32_t sector_count;
    int ret;

    if (channel == NULL || channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    priv = (struct znu_ext2_io_private*)channel->private_data;
    if (priv == NULL) {
        return EXT2_ET_BAD_DEVICE_NAME;
    }

    if (count == 0) {
        return 0;
    }

    lba = priv->base_lba + ((uint64_t)block * (uint64_t)priv->sectors_per_block);

    if (count > 0) {
        sector_count = (uint32_t)(count * priv->sectors_per_block);
    } else {
        sector_count = (uint32_t)priv->sectors_per_block;
    }

    ret = ahci_read_sectors(lba, sector_count, buf);
    if (ret != 0) {
        debugln("[ext2] read_blk: ahci_read_sectors failed at LBA %lu, count %u, ret=%d",
                (unsigned long)lba, sector_count, ret);
        //channel->read_error = ret;
        return EXT2_ET_IO_CHANNEL_CORRUPT;
    }

    return 0;
}

static errcode_t znu_ext2_io_write_blk(io_channel channel,
                                        unsigned long block,
                                        int count,
                                        const void* buf)
{
    struct znu_ext2_io_private* priv;
    uint64_t lba;
    uint32_t sector_count;
    int ret;

    if (channel == NULL || channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    priv = (struct znu_ext2_io_private*)channel->private_data;
    if (priv == NULL) {
        return EXT2_ET_BAD_DEVICE_NAME;
    }

    if (count == 0) {
        return 0;
    }

    lba = priv->base_lba + ((uint64_t)block * (uint64_t)priv->sectors_per_block);

    if (count > 0) {
        sector_count = (uint32_t)(count * priv->sectors_per_block);
    } else {
        sector_count = (uint32_t)priv->sectors_per_block;
    }

    ret = ahci_write_sectors(lba, sector_count, buf);
    if (ret != 0) {
        debugln("[ext2] write_blk: ahci_write_sectors failed at LBA %lu, count %u, ret=%d",
                (unsigned long)lba, sector_count, ret);
        //channel->write_error = ret;
        return EXT2_ET_IO_CHANNEL_CORRUPT;
    }

    return 0;
}

static errcode_t znu_ext2_io_flush(io_channel channel)
{
    if (channel == NULL || channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    debugln("[ext2] flush: cache flush requested (no-op for AHCI)");
    return 0;
}

static errcode_t znu_ext2_io_read_blk64(io_channel channel,
                                         unsigned long long block,
                                         int count,
                                         void* buf)
{
    struct znu_ext2_io_private* priv;
    uint64_t lba;
    uint32_t sector_count;
    int ret;

    if (channel == NULL || channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    priv = (struct znu_ext2_io_private*)channel->private_data;
    if (priv == NULL) {
        return EXT2_ET_BAD_DEVICE_NAME;
    }

    if (count == 0) {
        return 0;
    }

    lba = priv->base_lba + (block * (uint64_t)priv->sectors_per_block);

    if (count > 0) {
        sector_count = (uint32_t)(count * priv->sectors_per_block);
    } else {
        sector_count = (uint32_t)priv->sectors_per_block;
    }

    ret = ahci_read_sectors(lba, sector_count, buf);
    if (ret != 0) {
        debugln("[ext2] read_blk64: ahci_read_sectors failed at LBA %llu, count %u, ret=%d",
                (unsigned long long)lba, sector_count, ret);
        //channel->read_error = ret;
        return EXT2_ET_IO_CHANNEL_CORRUPT;
    }

    return 0;
}

static errcode_t znu_ext2_io_write_blk64(io_channel channel,
                                          unsigned long long block,
                                          int count,
                                          const void* buf)
{
    struct znu_ext2_io_private* priv;
    uint64_t lba;
    uint32_t sector_count;
    int ret;

    if (channel == NULL || channel->magic != EXT2_ET_MAGIC_IO_CHANNEL) {
        return EXT2_ET_MAGIC_IO_CHANNEL;
    }

    priv = (struct znu_ext2_io_private*)channel->private_data;
    if (priv == NULL) {
        return EXT2_ET_BAD_DEVICE_NAME;
    }

    if (count == 0) {
        return 0;
    }

    lba = priv->base_lba + (block * (uint64_t)priv->sectors_per_block);

    if (count > 0) {
        sector_count = (uint32_t)(count * priv->sectors_per_block);
    } else {
        sector_count = (uint32_t)priv->sectors_per_block;
    }

    ret = ahci_write_sectors(lba, sector_count, buf);
    if (ret != 0) {
        debugln("[ext2] write_blk64: ahci_write_sectors failed at LBA %llu, count %u, ret=%d",
                (unsigned long long)lba, sector_count, ret);
        //channel->write_error = ret;
        return EXT2_ET_IO_CHANNEL_CORRUPT;
    }

    return 0;
}

static errcode_t znu_ext2_io_set_option(io_channel channel,
                                         const char* option,
                                         const char* arg)
{
    (void)channel;
    (void)option;
    (void)arg;
    return EXT2_ET_UNIMPLEMENTED;
}

static errcode_t znu_ext2_io_get_stats(io_channel channel,
                                        io_stats* stats)
{
    (void)channel;
    (void)stats;
    return EXT2_ET_UNIMPLEMENTED;
}

static errcode_t znu_ext2_io_discard(io_channel channel,
                                      unsigned long long block,
                                      unsigned long long count)
{
    (void)channel;
    (void)block;
    (void)count;
    return EXT2_ET_UNIMPLEMENTED;
}

static errcode_t znu_ext2_io_zeroout(io_channel channel,
                                      unsigned long long block,
                                      unsigned long long count)
{
    (void)channel;
    (void)block;
    (void)count;
    return EXT2_ET_UNIMPLEMENTED;
}

static struct struct_io_manager znu_ext2_io_manager = {
    .magic = EXT2_ET_MAGIC_IO_MANAGER,
    .name = "znu_ahci_ext2",
    .open = znu_ext2_io_open,
    .close = znu_ext2_io_close,
    .set_blksize = znu_ext2_io_set_blksize,
    .read_blk = znu_ext2_io_read_blk,
    .write_blk = znu_ext2_io_write_blk,
    .flush = znu_ext2_io_flush,
    .read_blk64 = znu_ext2_io_read_blk64,
    .write_blk64 = znu_ext2_io_write_blk64,
    .set_option = znu_ext2_io_set_option,
    .get_stats = znu_ext2_io_get_stats,
    .discard = znu_ext2_io_discard,
    // .zeroout = znu_ext2_io_zeroout
};

static ext2_filsys znu_ext2_fs = NULL;
static io_channel znu_ext2_channel = NULL;

static errcode_t znu_ext2_malloc(unsigned long size, void** ptr)
{
    void* p = kmalloc((size_t)size);
    if (p == NULL) {
        debugln("[ext2] malloc: failed to allocate %lu bytes", size);
        return ENOMEM;
    }
    *ptr = p;
    return 0;
}

static errcode_t znu_ext2_realloc(void* old_ptr,
                                   unsigned long size,
                                   void** ptr)
{
    void* p = kmalloc((size_t)size);
    if (p == NULL) {
        debugln("[ext2] realloc: failed to allocate %lu bytes", size);
        return ENOMEM;
    }
    if (old_ptr != NULL) {
        memcpy(p, old_ptr, (size_t)size);
        kfree(old_ptr);
    }
    *ptr = p;
    return 0;
}

static void znu_ext2_free(void* ptr)
{
    if (ptr != NULL) {
        kfree(ptr);
    }
}

static int znu_ext2_mode_to_vfs_type(mode_t mode)
{
    if (LINUX_S_ISDIR(mode)) {
        return VFS_DIRECTORY;
    } else if (LINUX_S_ISREG(mode)) {
        return VFS_FILE;
    } else if (LINUX_S_ISLNK(mode)) {
        return VFS_FILE;
    } else {
        return VFS_FILE;
    }
}

static const char* znu_ext2_type_name(int type)
{
    switch (type) {
        case VFS_DIRECTORY: return "directory";
        case VFS_FILE:        return "file";
        default:              return "unknown";
    }
}

static int ext2_vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset)
{
    ext2_ino_t ino;
    ext2_file_t file;
    errcode_t retval;
    unsigned int bytes_read = 0;
    unsigned int total_read = 0;
    size_t remaining;
    size_t current_offset;
    char* dest;

    if (node == NULL || buf == NULL || size == 0) {
        return -1;
    }

    if (node->type != VFS_FILE) {
        debugln("[ext2] read: node '%s' is not a file (type=%d)",
                node->name, node->type);
        return -1;
    }

    ino = (ext2_ino_t)node->data;
    if (ino == 0) {
        debugln("[ext2] read: invalid inode for node '%s'", node->name);
        return -1;
    }

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] read: filesystem not initialized");
        return -1;
    }

    retval = ext2fs_file_open(znu_ext2_fs, ino, 0, &file);
    if (retval != 0) {
        debugln("[ext2] read: ext2fs_file_open failed for inode %u, retval=%d",
                (unsigned int)ino, retval);
        return -1;
    }

    retval = ext2fs_file_llseek(file, (ext2_off64_t)offset, EXT2_SEEK_SET, NULL);
    if (retval != 0) {
        debugln("[ext2] read: ext2fs_file_llseek failed for inode %u, retval=%d",
                (unsigned int)ino, retval);
        ext2fs_file_close(file);
        return -1;
    }

    remaining = size;
    current_offset = offset;
    dest = (char*)buf;

    while (remaining > 0) {
        unsigned int to_read = (remaining > 0xFFFFFFFFU) ? 0xFFFFFFFFU : (unsigned int)remaining;

        retval = ext2fs_file_read(file, dest + total_read, to_read, &bytes_read);
        if (retval != 0) {
            debugln("[ext2] read: ext2fs_file_read failed at offset %zu, retval=%d",
                    current_offset, retval);
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        total_read += bytes_read;
        remaining -= bytes_read;
        current_offset += bytes_read;
    }

    retval = ext2fs_file_close(file);
    if (retval != 0) {
        debugln("[ext2] read: ext2fs_file_close warning, retval=%d", retval);
    }

    debugln("[ext2] read: inode %u, offset %zu, requested %zu, read %u",
            (unsigned int)ino, offset, size, total_read);

    return (int)total_read;
}

static int ext2_vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset)
{
    ext2_ino_t ino;
    ext2_file_t file;
    errcode_t retval;
    unsigned int bytes_written = 0;
    unsigned int total_written = 0;
    size_t remaining;
    size_t current_offset;
    const char* src;

    if (node == NULL || buf == NULL || size == 0) {
        return -1;
    }

    if (node->type != VFS_FILE) {
        debugln("[ext2] write: node '%s' is not a file (type=%d)",
                node->name, node->type);
        return -1;
    }

    ino = (ext2_ino_t)node->data;
    if (ino == 0) {
        debugln("[ext2] write: invalid inode for node '%s'", node->name);
        return -1;
    }

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] write: filesystem not initialized");
        return -1;
    }

    retval = ext2fs_file_open(znu_ext2_fs, ino, EXT2_FILE_WRITE, &file);
    if (retval != 0) {
        debugln("[ext2] write: ext2fs_file_open failed for inode %u, retval=%d",
                (unsigned int)ino, retval);
        return -1;
    }

    retval = ext2fs_file_llseek(file, (ext2_off64_t)offset, EXT2_SEEK_SET, NULL);
    if (retval != 0) {
        debugln("[ext2] write: ext2fs_file_llseek failed for inode %u, retval=%d",
                (unsigned int)ino, retval);
        ext2fs_file_close(file);
        return -1;
    }

    remaining = size;
    current_offset = offset;
    src = (const char*)buf;

    while (remaining > 0) {
        unsigned int to_write = (remaining > 0xFFFFFFFFU) ? 0xFFFFFFFFU : (unsigned int)remaining;

        retval = ext2fs_file_write(file, src + total_written, to_write, &bytes_written);
        if (retval != 0) {
            debugln("[ext2] write: ext2fs_file_write failed at offset %zu, retval=%d",
                    current_offset, retval);
            break;
        }

        if (bytes_written == 0) {
            break;
        }

        total_written += bytes_written;
        remaining -= bytes_written;
        current_offset += bytes_written;
    }

    retval = ext2fs_file_flush(file);
    if (retval != 0) {
        debugln("[ext2] write: ext2fs_file_flush warning, retval=%d", retval);
    }

    retval = ext2fs_file_close(file);
    if (retval != 0) {
        debugln("[ext2] write: ext2fs_file_close warning, retval=%d", retval);
    }

    struct ext2_inode inode;
    retval = ext2fs_read_inode(znu_ext2_fs, ino, &inode);
    if (retval == 0) {
        node->size = (size_t)inode.i_size;
    }

    debugln("[ext2] write: inode %u, offset %zu, requested %zu, written %u",
            (unsigned int)ino, offset, size, total_written);

    return (int)total_written;
}

struct znu_readdir_ctx {
    znu_dirent_t* buffer;
    size_t count;
    uint32_t start_index;
    uint32_t current_index;
    uint32_t found_count;
    size_t max_count;
    int error;
};

static int znu_ext2_dir_iterate_callback(struct ext2_dir_entry* dirent,
                                          int offset,
                                          int blocksize,
                                          char* buf,
                                          void* priv_data)
{
    struct znu_readdir_ctx* ctx;
    struct ext2_dir_entry_2* dirent2;
    ext2_ino_t ino;
    size_t name_len;
    char name_buf[128];
    struct ext2_inode inode;
    int vfs_type;

    (void)offset;
    (void)blocksize;
    (void)buf;

    ctx = (struct znu_readdir_ctx*)priv_data;
    if (ctx == NULL) {
        return DIRENT_ABORT;
    }

    if (ctx->found_count >= ctx->max_count) {
        return DIRENT_ABORT;
    }

    dirent2 = (struct ext2_dir_entry_2*)dirent;
    ino = dirent2->inode;
    if (ino == 0) {
        return 0;
    }

    name_len = dirent2->name_len & 0xFF;
    if (name_len >= sizeof(name_buf)) {
        name_len = sizeof(name_buf) - 1;
    }

    memset(name_buf, 0, sizeof(name_buf));
    memcpy(name_buf, dirent2->name, name_len);

    if (strcmp(name_buf, ".") == 0 || strcmp(name_buf, "..") == 0) {
        return 0;
    }

    if (ctx->current_index < ctx->start_index) {
        ctx->current_index++;
        return 0;
    }

    vfs_type = VFS_FILE;
    if (ext2fs_read_inode(znu_ext2_fs, ino, &inode) == 0) {
        vfs_type = znu_ext2_mode_to_vfs_type(inode.i_mode);
    }

    memset(&ctx->buffer[ctx->found_count], 0, sizeof(znu_dirent_t));
    memcpy(ctx->buffer[ctx->found_count].name, name_buf, name_len);
    ctx->buffer[ctx->found_count].type = vfs_type;
    ctx->buffer[ctx->found_count].size = (size_t)inode.i_size;

    ctx->found_count++;
    ctx->current_index++;

    if (ctx->found_count >= ctx->max_count) {
        return DIRENT_ABORT;
    }

    return 0;
}

static int ext2_vfs_readdir(vfs_node_t* node,
                             uint32_t start_index,
                             void* buf,
                             size_t count)
{
    ext2_ino_t ino;
    errcode_t retval;
    struct znu_readdir_ctx ctx;

    if (node == NULL || buf == NULL || count == 0) {
        return -1;
    }

    if (node->type != VFS_DIRECTORY) {
        debugln("[ext2] readdir: node '%s' is not a directory (type=%d)",
                node->name, node->type);
        return -1;
    }

    ino = (ext2_ino_t)node->data;
    if (ino == 0) {
        debugln("[ext2] readdir: invalid inode for node '%s'", node->name);
        return -1;
    }

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] readdir: filesystem not initialized");
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.buffer = (znu_dirent_t*)buf;
    ctx.count = count;
    ctx.start_index = start_index;
    ctx.current_index = 0;
    ctx.found_count = 0;
    ctx.max_count = count / sizeof(znu_dirent_t);
    if (ctx.max_count == 0) {
        ctx.max_count = 1;
    }
    ctx.error = 0;

    retval = ext2fs_dir_iterate(znu_ext2_fs, ino, 0, NULL,
                                 znu_ext2_dir_iterate_callback, &ctx);
    if (retval != 0 && retval != EXT2_ET_DIR_NO_SPACE) {
        debugln("[ext2] readdir: ext2fs_dir_iterate failed for inode %u, retval=%d",
                (unsigned int)ino, retval);
        return -1;
    }

    debugln("[ext2] readdir: inode %u, start_index %u, returned %u entries",
            (unsigned int)ino, start_index, ctx.found_count);

    return (int)ctx.found_count;
}

static vfs_node_t* ext2_vfs_find(vfs_node_t* parent, const char* name)
{
    ext2_ino_t parent_ino;
    ext2_ino_t child_ino;
    errcode_t retval;
    struct ext2_inode inode;
    vfs_node_t* node;
    int vfs_type;

    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] find: filesystem not initialized");
        return NULL;
    }

    if (parent == NULL) {
        parent_ino = EXT2_ROOT_INO;
    } else {
        parent_ino = (ext2_ino_t)parent->data;
    }

    if (parent_ino == 0) {
        debugln("[ext2] find: invalid parent inode");
        return NULL;
    }

    retval = ext2fs_namei(znu_ext2_fs, parent_ino, parent_ino, name, &child_ino);
    if (retval != 0) {
        debugln("[ext2] find: ext2fs_namei failed for '%s' in inode %u, retval=%d",
                name, (unsigned int)parent_ino, retval);
        return NULL;
    }

    if (child_ino == 0) {
        debugln("[ext2] find: inode not found for '%s'", name);
        return NULL;
    }

    retval = ext2fs_read_inode(znu_ext2_fs, child_ino, &inode);
    if (retval != 0) {
        debugln("[ext2] find: ext2fs_read_inode failed for inode %u, retval=%d",
                (unsigned int)child_ino, retval);
        return NULL;
    }

    vfs_type = znu_ext2_mode_to_vfs_type(inode.i_mode);

    node = vfs_create_node(name, vfs_type);
    if (node == NULL) {
        debugln("[ext2] find: vfs_create_node failed for '%s'", name);
        return NULL;
    }

    node->size = (size_t)inode.i_size;
    node->data = (uintptr_t)child_ino;

    debugln("[ext2] find: resolved '%s' -> inode %u, type=%s, size=%zu",
            name, (unsigned int)child_ino, znu_ext2_type_name(vfs_type), node->size);

    return node;
}

static vfs_ops_t ext2_vfs_ops = {
    .read = ext2_vfs_read,
    .write = ext2_vfs_write,
    .readdir = ext2_vfs_readdir,
    .find_node = ext2_vfs_find
};

bool ext2_init_on_disk(void)
{
    errcode_t retval;
    struct znu_ext2_io_private* priv;
    int flags = EXT2_FLAG_RW;

    debugln("[ext2] init: starting ext2 filesystem initialization");

    if (znu_ext2_fs != NULL) {
        debugln("[ext2] init: filesystem already mounted");
        return false;
    }

    //ext2fs_set_generic_bitmap_padding(znu_ext2_malloc,
    //                                 znu_ext2_free,
    //                                 znu_ext2_realloc);

    retval = ext2fs_open(NULL, flags, 0, 0, &znu_ext2_io_manager, &znu_ext2_fs);
    if (retval != 0) {
        debugln("[ext2] init: ext2fs_open failed, retval=%d", retval);
        znu_ext2_fs = NULL;
        return false;
    }

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] init: ext2fs_open returned NULL filesystem");
        return false;
    }

    znu_ext2_channel = znu_ext2_fs->io;

    priv = (struct znu_ext2_io_private*)znu_ext2_channel->private_data;
    if (priv != NULL) {
        priv->block_size = (uint32_t)znu_ext2_fs->blocksize;
        priv->sectors_per_block = (uint32_t)(znu_ext2_fs->blocksize / 512);
        if (priv->sectors_per_block == 0) {
            priv->sectors_per_block = 1;
        }
        debugln("[ext2] init: block_size=%u, sectors_per_block=%u",
                priv->block_size, priv->sectors_per_block);
    }

    retval = ext2fs_read_block_bitmap(znu_ext2_fs);
    if (retval != 0) {
        debugln("[ext2] init: ext2fs_read_block_bitmap failed, retval=%d", retval);
        ext2fs_close(znu_ext2_fs);
        znu_ext2_fs = NULL;
        znu_ext2_channel = NULL;
        return false;
    }

    retval = ext2fs_read_inode_bitmap(znu_ext2_fs);
    if (retval != 0) {
        debugln("[ext2] init: ext2fs_read_inode_bitmap failed, retval=%d", retval);
        ext2fs_close(znu_ext2_fs);
        znu_ext2_fs = NULL;
        znu_ext2_channel = NULL;
        return false;
    }

    retval = ext2fs_read_inode_bitmap(znu_ext2_fs);
    if (retval != 0) {
        debugln("[ext2] init: inode bitmap re-read warning, retval=%d", retval);
    }

    debugln("[ext2] init: filesystem mounted successfully, blocksize=%ld, inodes=%u, blocks=%u",
            znu_ext2_fs->blocksize,
            znu_ext2_fs->super->s_inodes_count,
            znu_ext2_fs->super->s_blocks_count);

    return true;
}

vfs_node_t* ext2_get_root_node(void)
{
    vfs_node_t* root;
    struct ext2_inode inode;
    errcode_t retval;

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] get_root: filesystem not initialized");
        return NULL;
    }

    retval = ext2fs_read_inode(znu_ext2_fs, EXT2_ROOT_INO, &inode);
    if (retval != 0) {
        debugln("[ext2] get_root: ext2fs_read_inode failed for root, retval=%d", retval);
        return NULL;
    }

    root = vfs_create_node("/", VFS_DIRECTORY);
    if (root == NULL) {
        debugln("[ext2] get_root: vfs_create_node failed for root");
        return NULL;
    }

    root->size = (size_t)inode.i_size;
    root->data = (uintptr_t)EXT2_ROOT_INO;
    root->ops = &ext2_vfs_ops;

    debugln("[ext2] get_root: root node created, inode=%u, size=%zu",
            (unsigned int)EXT2_ROOT_INO, root->size);

    return root;
}

bool ext2_unmount(void)
{
    errcode_t retval;

    if (znu_ext2_fs == NULL) {
        debugln("[ext2] unmount: no filesystem mounted");
        return false;
    }

    retval = ext2fs_close(znu_ext2_fs);
    if (retval != 0) {
        debugln("[ext2] unmount: ext2fs_close warning, retval=%d", retval);
    }

    znu_ext2_fs = NULL;
    znu_ext2_channel = NULL;

    debugln("[ext2] unmount: filesystem unmounted");
    return true;
}

void ext2_print_superblock_info(void)
{
    if (znu_ext2_fs == NULL || znu_ext2_fs->super == NULL) {
        debugln("[ext2] info: filesystem not initialized");
        return;
    }

    struct ext2_super_block* sb = znu_ext2_fs->super;

    debugln("[ext2] info: === Ext2 Superblock ===");
    debugln("[ext2] info: inodes_count      = %u", sb->s_inodes_count);
    debugln("[ext2] info: blocks_count      = %u", sb->s_blocks_count);
    debugln("[ext2] info: r_blocks_count    = %u", sb->s_r_blocks_count);
    debugln("[ext2] info: free_blocks_count = %u", sb->s_free_blocks_count);
    debugln("[ext2] info: free_inodes_count = %u", sb->s_free_inodes_count);
    debugln("[ext2] info: first_data_block  = %u", sb->s_first_data_block);
    debugln("[ext2] info: block_size        = %u", 1024 << sb->s_log_block_size);
    debugln("[ext2] info: frag_size         = %u", 1024 << sb->s_log_cluster_size);
    debugln("[ext2] info: blocks_per_group  = %u", sb->s_blocks_per_group);
    //debugln("[ext2] info: frags_per_group   = %u", sb->s_frags_per_group);
    debugln("[ext2] info: inodes_per_group  = %u", sb->s_inodes_per_group);
    debugln("[ext2] info: magic             = 0x%04X", sb->s_magic);
    debugln("[ext2] info: state             = %u", sb->s_state);
    debugln("[ext2] info: rev_level         = %u", sb->s_rev_level);
    debugln("[ext2] info: first_ino         = %u", sb->s_first_ino);
    debugln("[ext2] info: inode_size        = %u", sb->s_inode_size);
    debugln("[ext2] info: ======================");
}
