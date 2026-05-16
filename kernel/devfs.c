#include <vfs.h>
#include <string.h>
#include <stdlib.h>
#include <devfs.h>

static vfs_node_t* dev_root = NULL;

// --- /dev/null Operations ---
static int null_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    (void)node; (void)buf; (void)size; (void)offset;
    return 0; // EOF immediately
}

static int null_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    (void)node; (void)buf; (void)offset;
    return (int)size; // Discard data, report complete success
}

static vfs_ops_t null_ops = {
    .read = null_read,
    .write = null_write,
    .find_node = NULL
};

// --- /dev/zero Operations ---
static int zero_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    (void)node; (void)offset;
    memset(buf, 0, size); // Fill buffer with null bytes
    return (int)size;
}

static int zero_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    (void)node; (void)buf; (void)offset;
    return (int)size; // Discard data, report complete success
}

static vfs_ops_t zero_ops = {
    .read = zero_read,
    .write = zero_write,
    .find_node = NULL
};

// --- DevFS Core ---
void devfs_init(void) {
    // 1. Setup the /dev root directory node
    dev_root = vfs_create_node("dev", VFS_DIRECTORY);
    if (!dev_root) return;
    vfs_add_child(root_node, dev_root);

    // 2. Instantiate and attach /dev/null
    vfs_node_t* null_node = vfs_create_node("null", VFS_FILE);
    if (null_node) {
        null_node->ops = &null_ops;
        vfs_add_child(dev_root, null_node);
    }

    // 3. Instantiate and attach /dev/zero
    vfs_node_t* zero_node = vfs_create_node("zero", VFS_FILE);
    if (zero_node) {
        zero_node->ops = &zero_ops;
        vfs_add_child(dev_root, zero_node);
    }
}

vfs_node_t* devfs_get_root(void) {
    return dev_root;
}

void devfs_register_tty(vfs_node_t* tty_node) {
    if (dev_root && tty_node) {
        vfs_add_child(dev_root, tty_node);
    }
}

