#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: insmod <module.ko>\n");
        return 1;
    }

    printf("DEBUG: argv address: %p\n", argv);
    printf("DEBUG: argv[0] value (the pointer): %p\n", argv[0]);
    printf("DEBUG: argv[1] value (the pointer): %p\n", argv[1]);

    printf("DEBUG: Attempting to open path: '%s'\n", argv[1]);

    char *ptr = argv[1];
    printf("DEBUG: Raw bytes at %p: %02x %02x %02x %02x\n", ptr, ptr[0], ptr[1], ptr[2], ptr[3]);

    printf("DEBUG: Checking memory at %p\n", argv[1]);

    volatile uint64_t *vptr = (volatile uint64_t *)argv[1];
    printf("DEBUG: Memory content: %lx\n", *vptr);

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Failed to open module");
        return 1;
    }

    /* finit_module(fd, params, flags) */
    /* params can be "" for no arguments */
    long ret = syscall(SYS_finit_module, fd, "", 0);

    if (ret != 0) {
        perror("insmod failed");
        close(fd);
        return 1;
    }

    printf("Module loaded successfully.\n");
    close(fd);
    return 0;
}
