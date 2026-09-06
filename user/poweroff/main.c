#include <stdio.h>
#include <syscall.h>

int main(void) {
    printf("znu: System is shutting down...\n");
    sys_shutdown();
    return 0;
}
