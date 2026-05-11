#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int main() {
    struct termios t;

    ioctl(0, 0x5401, &t);

    printf("before: %x\n", t.c_lflag);

    t.c_lflag &= ~(ICANON | ECHO);

    ioctl(0, 0x5402, &t);

    ioctl(0, 0x5401, &t);

    printf("after: %x\n", t.c_lflag);

    while (1) {
        char c;

        if (read(0, &c, 1) == 1) {
            printf("[%d]\n", c);
        }
    }
}
