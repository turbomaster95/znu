#include <stdio.h>
#include <string.h>

int main() {
    printf("Hello!\n");
    printf("---------------------------------\n");
    printf("      Starting Zinit v0.0.0      \n");
    printf("---------------------------------\n");
    char line[256];
    while (1) {
        printf("znu> ");
        if (!fgets(line, sizeof(line), 0)) continue;
        
        // Strip newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, "help") == 0) {
            printf("ls, reboot, mem\n");
        } else if (strcmp(line, "reboot") == 0) {
            // sys_reboot syscall
        } else if (strcmp(line, "ls") == 0) {
            // iterate VFS via syscalls (future)
            printf("init, shell\n");
        } else if (strlen(line) > 0) {
            printf("unknown: %s\n", line);
        }
    }
    return 0;
}

