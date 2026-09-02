// @devcom:device
// name: fb0
// type: VFS_FILE
// ops: devfs_fb0_ops

#define FB_IOCTL_GET_INFO  0x4600

typedef struct framebuffer_info {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bits_per_pixel;
    uint64_t size;
} framebuffer_info_t;

static int devfs_fb0_read(
    vfs_node_t* node,
    void* buf,
    size_t size,
    size_t offset
) {
    (void)node;

    if (buf == NULL)
        return -1;

    if (framebuffer_addr == NULL)
        return -1;

    if (offset >= framebuffer_size)
        return 0;

    size_t available = framebuffer_size - offset;

    if (size > available)
        size = available;

    memcpy(
        buf,
        framebuffer_addr + offset,
        size
    );

    return (int)size;
}


static int devfs_fb0_write(
    vfs_node_t* node,
    const void* buf,
    size_t size,
    size_t offset
) {
    (void)node;

    if (buf == NULL)
        return -1;

    if (framebuffer_addr == NULL)
        return -1;

    if (offset >= framebuffer_size)
        return 0;

    size_t available = framebuffer_size - offset;

    if (size > available)
        size = available;

    memcpy(
        framebuffer_addr + offset,
        buf,
        size
    );

    return (int)size;
}


static int devfs_fb0_ioctl(
    vfs_node_t* node,
    unsigned long request,
    void* argp
) {
    (void)node;

    if (argp == NULL)
        return -1;

    switch (request) {
        case FB_IOCTL_GET_INFO: {
            framebuffer_info_t info = {
                .width = framebuffer_width,
                .height = framebuffer_height,
                .pitch = framebuffer_pitch,
                .bits_per_pixel = framebuffer_bpp,
                .size = framebuffer_size
            };

            memcpy(argp, &info, sizeof(info));
            return 0;
        }

        default:
            return -1;
    }
}


static vfs_ops_t devfs_fb0_ops = {
    .read = devfs_fb0_read,
    .write = devfs_fb0_write,
    .readdir = NULL,
    .create = NULL,
    .find_node = NULL,
    .ioctl = devfs_fb0_ioctl
};
