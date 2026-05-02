#include <stdio.h>
#include <string.h>
#include <syscall.h>

int main() {
    char buf[256];
    printf("Fake Shell Started\n");
    while (1) {
        printf("fake# ");
        readline(buf, sizeof(buf));
        printf("You typed: %s", buf);
        if (strcmp(buf, "exit\n") == 0) break;
        if (strcmp(buf, "quit\n") == 0) break;
    }
    return 0;
}
