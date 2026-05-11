#include <vfs.h>
#include <string.h>
#include <stdlib.h>
#include <devfs.h>

static vfs_node_t* dev_root;

void devfs_init(void) {
	vfs_node_t* dev = vfs_create_node("dev", VFS_DIRECTORY);
	vfs_add_child(root_node, dev);
}

vfs_node_t* devfs_get_root(void) {
    return dev_root;
}

void devfs_register_tty(vfs_node_t* tty_node) {
    vfs_add_child(dev_root, tty_node);
}
