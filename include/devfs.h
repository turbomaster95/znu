#ifndef DEVFS_H
#define DEVFS_H

#include <vfs.h>

void devfs_init(void);
vfs_node_t* devfs_get_root(void);
void devfs_register_tty(vfs_node_t* tty_node);

#endif
