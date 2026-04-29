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
	} else if (strncmp(line, "ls", 2) == 0) {
            char path[256];
            const char* input_path = line + 2;

            // Skip leading spaces
            while (*input_path == ' ') input_path++;

            // If no path provided, default to root
            if (*input_path == '\0') {
                strcpy(path, "/");
            } 
            // If it doesn't start with '/', prepend one
            else if (*input_path != '/') {
                path[0] = '/';
                strcpy(path + 1, input_path);
            } 
            // Otherwise, just copy it
            else {
                strcpy(path, input_path);
            }

            int fd = sys_open(path, 0);
            if (fd >= 0) {
                znu_dirent_t dents[16];
                int bytes_read = sys_getdents(fd, dents, sizeof(dents));
                
                if (bytes_read > 0) {
                    int count = bytes_read / sizeof(znu_dirent_t);
                    for (int i = 0; i < count; i++) {
                        printf("%s%s [%d bytes]\n", 
                            dents[i].name, 
                            (dents[i].type == 2 ? " (dir)" : ""), 
                            dents[i].size);
                    }
                }
                sys_close(fd);
            } else {
                printf("ls: could not open '%s'\n", path);
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
