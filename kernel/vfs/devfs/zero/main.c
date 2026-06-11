// @devcom:device
// name: zero
// type: VFS_FILE
// ops: devfs_zero_ops

// no need for includes as we are being included ourself :)

// --- /dev/zero Operations ---
static int devfs_zero_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    (void)node; (void)offset;
    memset(buf, 0, size); // Fill buffer with null bytes
    return (int)size;
}

static int devfs_zero_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    (void)node; (void)buf; (void)offset;
    return (int)size; // Discard data, report complete success
}

static vfs_ops_t devfs_zero_ops = {
    .read = devfs_zero_read,
    .write = devfs_zero_write,
    .find_node = NULL
};
