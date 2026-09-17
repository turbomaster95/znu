// @procom:node
// name: config
// type: VFS_FILE
// ops: procfs_config_ops
// size: (uintptr_t)kernel_config_data_end - (uintptr_t)kernel_config_data

#include <vfs.h>
#include <string.h>
#include <stdint.h>

asm (
"       .pushsection .rodata, \"a\"\n"
"       .ascii \"CFG_ST\"\n"
"       .global kernel_config_data\n"
"kernel_config_data:\n"
"       .incbin \"kernel/vfs/procfs/configd\"\n"
"       .global kernel_config_data_end\n"
"kernel_config_data_end:\n"
"       .ascii \"CFG_ED\"\n"
"       .popsection\n"
);

extern char kernel_config_data[];
extern char kernel_config_data_end[];

static int config_read(vfs_node_t *node, void* buf, size_t size, size_t offset) {
    (void)node;

    uint64_t total_size = (uintptr_t)kernel_config_data_end - (uintptr_t)kernel_config_data;

    if (offset >= total_size) return 0;

    if (offset + size > total_size) {
        size = total_size - offset;
    }

    memcpy(buf, kernel_config_data + offset, size);

    return size;
}

vfs_ops_t procfs_config_ops = {
    .read = config_read,
};
