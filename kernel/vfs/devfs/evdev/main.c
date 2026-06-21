#include <input.h>
#include <stdint.h>
#include <stddef.h>
#include <vfs.h>
#include <page.h>

extern vfs_node_t* dev_root;
vfs_node_t* ev_root = NULL;

static int devfs_evdev_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    (void)offset;
    evdev_state_t* state = (evdev_state_t*)node->internal_data;
    
    size_t event_count = size / sizeof(struct input_event);
    size_t read = 0;

    while (read < event_count && state->head != state->tail) {
        ((struct input_event*)buf)[read] = state->buffer[state->tail];
        state->tail = (state->tail + 1) % EV_BUFFER_SIZE;
        read++;
    }

    return (int)(read * sizeof(struct input_event));
}

void evdev_push_event(vfs_node_t* node, struct input_event ev) {
    evdev_state_t* state = (evdev_state_t*)node->internal_data;
    
    state->buffer[state->head] = ev;
    state->head = (state->head + 1) % EV_BUFFER_SIZE;
    
    // If we wrapped around, we drop the oldest event
    if (state->head == state->tail) {
        state->tail = (state->tail + 1) % EV_BUFFER_SIZE;
    }
}

vfs_ops_t devfs_evdev_ops = {
    .read = devfs_evdev_read,
    .write = NULL,
    .find_node = NULL
};

void evdev_init() {
    ev_root = vfs_create_node("input", VFS_DIRECTORY);
    vfs_add_child(dev_root, ev_root);
    evdev_state_t* state = kmalloc(sizeof(evdev_state_t));
    memset(state, 0, sizeof(evdev_state_t));

    vfs_node_t* in1 = vfs_create_node("event1", VFS_DEVICE);
    vfs_add_child(ev_root, in1);
    in1->internal_data = state;
    in1->ops = &devfs_evdev_ops;
}
