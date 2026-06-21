#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <input.h>

int main() {
    int fd = open("/dev/input/event1", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    struct input_event ev;
    while (read(fd, &ev, sizeof(struct input_event)) > 0) {
        printf("Event: Type=%d, Code=%d, Value=%d\n", ev.type, ev.code, ev.value);
    }

    close(fd);
    return 0;
}
