// @devcom:device
// name: null
// type: VFS_FILE
// ops: devfs_null_ops

// no need for includes as we are not being compiled individually :)

static int devfs_null_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    (void)node; (void)buf; (void)size; (void)offset;
    return 0; // EOF immediately
}

static int devfs_null_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    (void)node; (void)buf; (void)offset;
    return (int)size; // Discard data, report complete success
}

static vfs_ops_t devfs_null_ops = {
    .read = devfs_null_read,
    .write = devfs_null_write,
    .find_node = NULL
};
