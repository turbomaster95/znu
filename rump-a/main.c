#include <stdio.h>
#include <rump/rump.h>

int main() {
    printf("[Host] Starting Rump Kernel initialization...\n");

    /* Initialize the Rump microkernel */
    int error = rump_init();
    
    if (error != 0) {
        fprintf(stderr, "[Host] Initialization failed with error: %d\n", error);
        return 1;
    }

    printf("[Host] Rump Kernel is READY.\n");
    return 0;
}
