#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <sys/time.h>
#include <vfs.h>

#define EV_BUFFER_SIZE 64

struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

typedef struct {
    struct input_event buffer[EV_BUFFER_SIZE];
    int head; // Where we write
    int tail; // Where we read
} evdev_state_t;

#define EV_SYN      0x00
#define EV_KEY      0x01
#define EV_REL      0x02
#define EV_ABS      0x03

// Synchronization events
#define SYN_REPORT  0x00
#define SYN_CONFIG  0x01

void evdev_init(void);
void keyboard_init_evdev(void);
void evdev_push_event(vfs_node_t* node, struct input_event ev);

#endif
