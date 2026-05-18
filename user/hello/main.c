#include <stdio.h>
#include <syscall.h>

int main() {
    printf("Hello from a standalone program!\n");
    fflush(stdout);
    return 0;
}
