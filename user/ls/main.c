#include <stdio.h>
#include <string.h>
#include <syscall.h>

// Assuming this matches your kernel's dirent structure
//typedef struct {
//    uint32_t inode;
//    uint32_t size;
//    uint8_t type;
//    char name[256];
//} znu_dirent_t;

int main(int argc, char** argv) {
    char path[256];
    
    // If no argument, list root. Otherwise list argv[1]
    if (argc < 2) {
        strcpy(path, "/");
    } else {
        strcpy(path, argv[1]);
    }

    int fd = sys_open(path, 0);
    if (fd < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return 1;
    }

    znu_dirent_t dents[16];
    int bytes_read;

    // Keep reading until getdents returns 0
    while ((bytes_read = sys_getdents(fd, dents, sizeof(dents))) > 0) {
        int count = bytes_read / sizeof(znu_dirent_t);
        for (int i = 0; i < count; i++) {
            // ANSI Colors: Blue for dir (1;34), Green for file (1;32)
            if (dents[i].type == 2) { 
                printf("\033[1;34m%s\033[0m (dir)\n", dents[i].name);
            } else {
                printf("\033[1;32m%s\033[0m [%d bytes]\n", dents[i].name, dents[i].size);
            }
        }
    }

    sys_close(fd);
    return 0;
}
