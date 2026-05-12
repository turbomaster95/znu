#include <stdio.h>
#include <string.h>
#include <syscall.h>

void run_config() {
    int fd = sys_open("/etc/zinit.conf", 0);
    if (fd < 0) return;
    
    char buf[1024];
    int n = sys_read(fd, buf, 1023);
    if (n > 0) {
        buf[n] = '\0';
        char* line = buf;
        while (*line) {
            char* end = line;
            while (*end && *end != '\n') end++;
            bool has_next = (*end == '\n');
            *end = '\0';
            
            if (*line != '#' && *line != '\0') {
                printf("zInit: Spawning %s -i...\n", line);
                char* argv[] = { line, "-i", NULL };
                int pid = sys_spawn(line, argv, NULL);
                int status = 0;
                int waited_pid = sys_wait(pid, &status);
                printf("[zInit] Process %d exited with status %d\n", waited_pid, status);
            }
            
            if (!has_next) break;
            line = end + 1;
        }
    }
    sys_close(fd);
}

int main() {
    printf("Hello!\n");
    printf("---------------------------------\n");
    printf("      Starting zInit v0.1.0      \n");
    printf("---------------------------------\n");
    
    run_config();

    char line[256];
    while (1) {
        printf("\033[1;34mznu\033[0m \033[1;36m/\033[0m > ");
        readline(line, sizeof(line));
        
        // Strip newline for comparison
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, "help") == 0) {
            printf("Commands: ls, cat <file>, reboot, mem, shutdown, echo <text>, clear, <program>\n");
        } else if (strcmp(line, "reboot") == 0) {
            sys_reboot();
        } else if (strcmp(line, "shutdown") == 0) {
            sys_shutdown();
        } else if (strcmp(line, "clear") == 0) {
            printf("\033[2J\033[H");
        } else if (strncmp(line, "ls", 2) == 0) {
            // ... existing ls code ...
            char path[256];
            const char* input_path = line + 2;
            while (*input_path == ' ') input_path++;
            if (*input_path == '\0') strcpy(path, "/");
            else if (*input_path != '/') { path[0] = '/'; strcpy(path + 1, input_path); }
            else strcpy(path, input_path);

            int fd = sys_open(path, 0);
            if (fd >= 0) {
                znu_dirent_t dents[16];
                int bytes_read = sys_getdents(fd, dents, sizeof(dents));
                if (bytes_read > 0) {
                    int count = bytes_read / sizeof(znu_dirent_t);
                    for (int i = 0; i < count; i++) {
                        if (dents[i].type == 2) { // Directory
                            printf("\033[1;34m%s\033[0m (dir)\n", dents[i].name);
                        } else {
                            printf("\033[1;32m%s\033[0m [%d bytes]\n", dents[i].name, dents[i].size);
                        }
                    }
                }
                sys_close(fd);
            } else printf("ls: could not open '%s'\n", path);
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
            } else printf("Could not open %s\n", path);
        } else if (strcmp(line, "mem") == 0) {
            struct sysinfo info;
            if (sys_sysinfo(&info) == 0) {
                printf("Memory usage:\n");
                printf("  Total: %ld MB\n", info.totalram / 1024 / 1024);
                printf("  Free:  %ld MB\n", info.freeram / 1024 / 1024);
                printf("  Used:  %ld MB\n", (info.totalram - info.freeram) / 1024 / 1024);
                printf("  Procs: %d\n", info.procs);
            } else printf("sysinfo failed\n");
        } else if (strncmp(line, "echo ", 5) == 0) {
            printf("%s\n", line + 5);
        } else if (strcmp(line, "exit") == 0) {
            sys_shutdown();
	    sys_exit(0);
        } else if (strlen(line) > 0) {
            // Try to spawn as a program
            char* argv[] = { line, NULL };
            int pid = sys_spawn(line, argv, NULL);
            if (pid < 0) {
                char path[256];
                // Try /bin/
                strcpy(path, "/bin/");
                strcat(path, line);
                char* b_argv[] = { path, NULL };
                pid = sys_spawn(path, b_argv, NULL);
                
                if (pid < 0) {
                    // Try /sbin/
                    strcpy(path, "/sbin/");
                    strcat(path, line);
                    char* s_argv[] = { path, NULL };
                    pid = sys_spawn(path, s_argv, NULL);
                }
            }
            
            if (pid >= 0) {
                // printf("[zInit] Spawned %s (PID: %d)\n", line, pid);
                int status = 0;
                int waited_pid = sys_wait(pid, &status);
                // printf("[zInit] Process %d exited with status %d\n", waited_pid, status);
            } else {
                printf("?: %s\n", line);
            }
        }
    }
    return 0;
}
