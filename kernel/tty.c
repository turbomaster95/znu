#include <termios.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <proc.h>
#include <devfs.h>
#include <kernel/tty.h>
#include <stdio.h>
#include <page.h>

extern void terminal_putchar(char c);
extern void debug_putcharn(char c);

#define MAX_TTYS 2

static tty_t ttys[MAX_TTYS];
tty_device_t tty_devices[MAX_TTYS];

int active_tty = 0;

static vfs_node_t tty_nodes[MAX_TTYS];

static inline int tty_buffer_empty(tty_t *tty) {
    return tty->cooked_head == tty->cooked_tail;
}

static inline tty_t *tty_get(int id) {
    if (id < 0 || id >= MAX_TTYS)
        return &ttys[0];

    return &ttys[id];
}

static void tty_wake_readers(tty_t *tty) {
    tty_waiter_t *w = tty->readers;

    while (w) {
        if (w->proc) {
            w->proc->state = TASK_READY;
        }
        w = w->next;
    }

    tty->readers = NULL;
}

static int tty_vfs_read(vfs_node_t* node,
                        void* buf,
                        size_t size,
                        size_t offset) {
    (void)offset;

    tty_device_t* dev = (tty_device_t*)node->internal_data;
    
    if (!dev || !dev->tty) return -1;

    tty_t* tty = dev->tty;

    if (!tty) return -1;

    size_t got = 0;

    while (tty->cooked_head == tty->cooked_tail) {

        if (current_process)
            current_process->state = TASK_WAITING;

        __asm__ volatile("sti; hlt");
    }

    while (got < size &&
           tty->cooked_head != tty->cooked_tail) {

        ((char*)buf)[got++] =
            tty->cooked_buf[tty->cooked_tail];

        tty->cooked_tail =
            (tty->cooked_tail + 1) % TTY_BUF_SIZE;
    }

    return (int)got;
}

static int tty_vfs_write(vfs_node_t* node,
                         const void* buf,
                         size_t size,
                         size_t offset) {
    (void)offset;

    tty_device_t* dev = (tty_device_t*)node->internal_data;

    if (!dev || !dev->tty) return -1;

    tty_t* tty = dev->tty;

    if (!tty) return -1;

    const char* cbuf = buf;

    for (size_t i = 0; i < size; i++) {
        terminal_putchar(cbuf[i]);
	debug_putcharn(cbuf[i]);
    }

    return (int)size;
}

vfs_ops_t tty_vfs_ops = {
    .read = tty_vfs_read,
    .write = tty_vfs_write
};


void tty_init(void) {
    memset(ttys, 0, sizeof(ttys));
    memset(tty_devices, 0, sizeof(tty_devices));

    vfs_node_t* dev = vfs_path_to_node("/dev");

    for (int i = 0; i < MAX_TTYS; i++) {

        tty_t* tty = &ttys[i];

        tty->termios.c_lflag = ICANON | ECHO | ISIG;
        tty->termios.c_iflag = ICRNL;
        tty->termios.c_oflag = OPOST;

        tty_device_t* devobj = &tty_devices[i];

	devobj->tty = tty;

        vfs_node_t* node = &tty_nodes[i];

        memset(node, 0, sizeof(vfs_node_t));

        snprintf(node->name, sizeof(node->name),
                 "tty%d", i);

        node->type = VFS_DEVICE;
        node->ops = &tty_vfs_ops;
        node->internal_data = devobj;

        devobj->node = node;

        vfs_add_child(dev, node);
    }
}

void tty_input_char(int id, char c) {
    tty_t *tty = &ttys[id];
 
    /* normalize CR */
    if (c == '\r')
        c = '\n';

    if (c == '\b' || c == 127) {
        if (tty->line_len > 0) {
            tty->line_len--;

            if (tty->termios.c_lflag & ECHO) {
                terminal_putchar('\b');
		debug_putcharn('\b');
                terminal_putchar(' ');
                debug_putcharn(' ');
                terminal_putchar('\b');
                debug_putcharn('\b');
            }
        }
        return;
    }

    if (tty->termios.c_lflag & ECHO) {
        terminal_putchar(c);
	debug_putcharn(c);
    }

    if (tty->line_len >= (TTY_BUF_SIZE - 1)) {
        tty->line_len = 0;
        return;
    }

    tty->line_buf[tty->line_len++] = c;

    if (c == '\n') {

        for (size_t i = 0; i < tty->line_len; i++) {

            size_t next = (tty->cooked_head + 1) % TTY_BUF_SIZE;

            if (next == tty->cooked_tail)
                break;

            tty->cooked_buf[tty->cooked_head] =
                tty->line_buf[i];

            tty->cooked_head = next;
        }

        tty->line_len = 0;

        tty_wake_readers(tty);
    }
}

size_t tty_read(char *buf, size_t count) {
    tty_t *tty = tty_get(active_tty);
    size_t got = 0;

    if (!buf || count == 0)
        return 0;

    while (tty_buffer_empty(tty)) {

        if (current_process) {
            current_process->state = TASK_WAITING;
        }

        __asm__ volatile("sti; hlt");
    }

    while (got < count &&
           tty->cooked_head != tty->cooked_tail) {

        buf[got++] =
            tty->cooked_buf[tty->cooked_tail];

        tty->cooked_tail =
            (tty->cooked_tail + 1) % TTY_BUF_SIZE;
    }

    return got;
}

long tty_write(const char *buf, size_t count) {
    if (!buf)
        return -1;

    for (size_t i = 0; i < count; i++) {
        terminal_putchar(buf[i]);
        debug_putcharn(buf[i]);
    }
    return (long)count;
}

int tty_ioctl(unsigned long request, void *argp) {
    tty_t *tty = tty_get(active_tty);
    if (!argp)
        return -1;

    switch (request) {

        case 0x5401: /* TCGETS */
            memcpy(argp, &tty->termios, sizeof(struct termios));
            return 0;

        case 0x5402: /* TCSETS */
        case 0x5403:
        case 0x5404:
            memcpy(&tty->termios, argp, sizeof(struct termios));
            return 0;

        case 0x5413: { /* TIOCGWINSZ */
            uint16_t *ws = (uint16_t *)argp;

            ws[0] = 25;
            ws[1] = 80;
            ws[2] = 0;
            ws[3] = 0;

            return 0;
        }
    }

    return -1;
}

vfs_file_t* tty_open_file(int id, int flags) {
    if (id < 0 || id >= MAX_TTYS)
        return NULL;

    vfs_file_t* file = kmalloc(sizeof(vfs_file_t));
    if (!file)
        return NULL;

    memset(file, 0, sizeof(vfs_file_t));

    file->node = tty_devices[id].node;
    file->flags = flags;
    file->pos = 0;

    return file;
}
