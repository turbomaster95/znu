#include <stdio.h>
#include <string.h>
#include <syscall.h>

int main() {
    printf("Hello!\n");
    printf("---------------------------------\n");
    printf("      Starting Zinit v0.0.0      \n");
    printf("---------------------------------\n");
    char line[256];
    while (1) {
        printf("znu> ");
        readline(line, sizeof(line));
        
        // Strip newline for comparison
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, "help") == 0) {
            printf("Commands: ls, cat <file>, reboot, mem, shutdown, echo <text>, clear\n");
        } else if (strcmp(line, "reboot") == 0) {
            sys_reboot();
        } else if (strcmp(line, "shutdown") == 0) {
            sys_shutdown();
        } else if (strcmp(line, "clear") == 0) {
            printf("\033[2J\033[H");
        } else if (strcmp(line, "ls") == 0) {
            int fd = sys_open("/", 0);
            if (fd >= 0) {
                znu_dirent_t dents[16];
                int n = sys_getdents(fd, dents, sizeof(dents));
                for (int i = 0; i < n / sizeof(znu_dirent_t); i++) {
                    printf("%s  ", dents[i].name);
                    if (dents[i].type == 2) printf("(dir)  ");
                    printf("[%d bytes]\n", dents[i].size);
                }
                sys_close(fd);
            } else {
                printf("Error opening /\n");
            }
        } else if (strncmp(line, "cat ", 4) == 0) {
            const char* path = line + 4;
            int fd = sys_open(path, 0);
            if (fd >= 0) {
                char buf[512];
                int n;
                while ((n = sys_read(fd, buf, sizeof(buf) - 1)) > 0) {
                    buf[n] = '\0';
                    printf("%s", buf);
                }
                printf("\n");
                sys_close(fd);
            } else {
                printf("Could not open %s\n", path);
            }
        } else if (strcmp(line, "mem") == 0) {
            struct sysinfo info;
            if (sys_sysinfo(&info) == 0) {
                printf("Memory usage:\n");
                printf("  Total: %d MB\n", info.totalram / 1024 / 1024);
                printf("  Free:  %d MB\n", info.freeram / 1024 / 1024);
                printf("  Used:  %d MB\n", (info.totalram - info.freeram) / 1024 / 1024);
                printf("  Procs: %d\n", info.procs);
            } else {
                printf("sysinfo failed\n");
            }
        } else if (strncmp(line, "echo ", 5) == 0) {
            printf("%s\n", line + 5);
        } else if (strlen(line) > 0) {
            printf("?: %s\n", line);
        }
    }
    return 0;
}
