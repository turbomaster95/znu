// @procom:node
// name: kallsyms
// type: VFS_FILE
// ops: procfs_kallsyms_ops
// size: kallsyms_size_bytes()

#include <symbols.h>
#include <vfs.h>
#include <string.h>
#include <stdint.h>

static int kallsyms_read(vfs_node_t *node, void* buf, size_t size, size_t offset) {
    (void)node;
    return (int)kallsyms_dump_range((char*)buf, size, offset);
}

vfs_ops_t procfs_kallsyms_ops = {
    .read = kallsyms_read,
};
