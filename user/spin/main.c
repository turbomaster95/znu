#include <unistd.h>
#include <stdint.h>

int main(void) {
    int pid = fork();

    volatile uint64_t x = 0;

    while (1) {
        x++;
    }

    return 0;
}
