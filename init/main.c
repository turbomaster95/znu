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
            printf("ls, reboot, mem, shutdown, echo <text>\n");
        } else if (strcmp(line, "reboot") == 0) {
            sys_reboot();
        } else if (strcmp(line, "shutdown") == 0) {
//            sys_shutdown();
        } else if (strcmp(line, "ls") == 0) {
            printf("init shell\n");
        } else if (strncmp(line, "echo ", 5) == 0) {
            printf("%s\n", line + 5);
        } else if (strlen(line) > 0) {
            printf("?: %s\n", line);
        }
    }
    return 0;
}
