#include <vfs.h>
#include <stdint.h>
#include <string.h>

struct cpio_header {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
};

static uint32_t hex8_to_u32(char *s) {
    uint32_t res = 0;
    for (int i = 0; i < 8; i++) {
        res <<= 4;
        if (s[i] >= '0' && s[i] <= '9') res += s[i] - '0';
        else if (s[i] >= 'A' && s[i] <= 'F') res += s[i] - 'A' + 10;
        else if (s[i] >= 'a' && s[i] <= 'f') res += s[i] - 'a' + 10;
    }
    return res;
}

void cpio_parse(void *addr) {
    uint8_t *ptr = (uint8_t*)addr;

    while (1) {
        struct cpio_header *h = (struct cpio_header*)ptr;

        if (strncmp(h->magic, "070701", 6) != 0) break;

        uint32_t filesize = hex8_to_u32(h->filesize);
        uint32_t namesize = hex8_to_u32(h->namesize);
        uint32_t mode     = hex8_to_u32(h->mode);

        char *filename = (char*)(ptr + sizeof(struct cpio_header));

        // Stop at trailer
        if (strncmp(filename, "TRAILER!!!", 10) == 0) break;

        // Skip device files, symlinks, etc. for now
        uint32_t file_type = mode & 0xF000;
        if (file_type != 0x8000 && file_type != 0x4000) {
            // Not a regular file or directory — skip
            ptr = ptr + ((sizeof(struct cpio_header) + namesize + 3) & ~3);
            ptr = ptr + ((filesize + 3) & ~3);
            continue;
        }

        // Data pointer
        uint8_t *data = ptr + ((sizeof(struct cpio_header) + namesize + 3) & ~3);

        // Normalize path: strip leading "./" and ensure leading "/"
        char path_buf[256];
        const char* src = filename;
        
        // Skip leading "./"
        while (src[0] == '.' && src[1] == '/') src += 2;
        
        if (src[0] == '/') {
            strncpy(path_buf, src, 255);
        } else {
            path_buf[0] = '/';
            strncpy(path_buf + 1, src, 254);
        }
        path_buf[255] = '\0';

        if (file_type == 0x8000) { // Regular file
            vfs_register_file(path_buf, (uintptr_t)data, filesize);
        } else if (file_type == 0x4000) { // Directory
            // Ensure directory exists in tree
            vfs_node_t* existing = vfs_path_to_node(path_buf);
            if (!existing) {
                vfs_register_file(path_buf, 0, 0);
                // Fix type to directory (vfs_register_file creates files)
                vfs_node_t* dir = vfs_path_to_node(path_buf);
                if (dir) dir->type = VFS_DIRECTORY;
            }
        }

        ptr = data + ((filesize + 3) & ~3);
    }
}
