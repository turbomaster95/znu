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

tty_t ttys[MAX_TTYS];
tty_device_t tty_devices[MAX_TTYS];

static vfs_node_t tty_nodes[MAX_TTYS];

int active_tty = 0;

static inline bool tty_buf_empty(tty_t* tty) {
    return tty->cooked_head == tty->cooked_tail;
}

static inline bool tty_buf_full(tty_t* tty) {
    return ((tty->cooked_head + 1) % TTY_BUF_SIZE)
        == tty->cooked_tail;
}

static inline void tty_buf_put(tty_t* tty, char c) {
    if (tty_buf_full(tty))
        return;

    tty->cooked_buf[tty->cooked_head] = c;
    tty->cooked_head =
        (tty->cooked_head + 1) % TTY_BUF_SIZE;
}

static inline char tty_buf_get(tty_t* tty) {
    char c = tty->cooked_buf[tty->cooked_tail];

    tty->cooked_tail =
        (tty->cooked_tail + 1) % TTY_BUF_SIZE;

    return c;
}

static void tty_wake_reader(tty_t* tty) {
    if (tty->waiting_reader) {
        tty->waiting_reader->state = TASK_READY;
        tty->waiting_reader = NULL;
    }
}

size_t tty_read(tty_t* tty, char* buf, size_t count) {
    if (!tty || !buf || count == 0)
        return 0;

    size_t got = 0;

    while (got < count) {

        while (tty_buf_empty(tty)) {

            if (!current_process)
                break;

            tty->waiting_reader = current_process;

            current_process->state = TASK_WAITING;

            asm volatile("sti");
            asm volatile("hlt");
            asm volatile("cli");
        }

        if (tty_buf_empty(tty))
            break;

        buf[got++] = tty_buf_get(tty);

        if (tty->termios.c_lflag & ICANON) {
            if (buf[got - 1] == '\n')
                break;
        }
    }

    return got;
}

long tty_write(tty_t* tty, const char* buf, size_t count) {
    if (!tty || !buf)
        return -1;

    for (size_t i = 0; i < count; i++) {
        char c = buf[i];

        if (c == '\n')
            terminal_putchar('\r');
	    
        terminal_putchar(c);
    }

    return (long)count;
}

static int tty_vfs_read(vfs_node_t* node,
                        void* buf,
                        size_t size,
                        size_t offset) {
    (void)offset;

    if (!node || !buf)
        return -1;

    tty_device_t* dev =
        (tty_device_t*)node->internal_data;

    if (!dev || !dev->tty)
        return -1;

    return (int)tty_read(dev->tty, buf, size);
}

static int tty_vfs_write(vfs_node_t* node,
                         const void* buf,
                         size_t size,
                         size_t offset) {
    (void)offset;

    if (!node || !buf)
        return -1;

    tty_device_t* dev =
        (tty_device_t*)node->internal_data;

    if (!dev || !dev->tty)
        return -1;

    return (int)tty_write(
        dev->tty,
        (const char*)buf,
        size
    );
}

static vfs_ops_t tty_vfs_ops = {
    .read = tty_vfs_read,
    .write = tty_vfs_write
};

void tty_init(void) {
    memset(ttys, 0, sizeof(ttys));
    memset(tty_devices, 0, sizeof(tty_devices));
    memset(tty_nodes, 0, sizeof(tty_nodes));

    vfs_node_t* dev = vfs_path_to_node("/dev");

    for (int i = 0; i < MAX_TTYS; i++) {

        tty_t* tty = &ttys[i];

        tty->termios.c_lflag =
            ICANON | ECHO | ISIG;

        tty->termios.c_iflag = ICRNL;

        tty->termios.c_oflag = OPOST;

        tty_device_t* devobj =
            &tty_devices[i];

        devobj->tty = tty;

        vfs_node_t* node =
            &tty_nodes[i];

        snprintf(
            node->name,
            sizeof(node->name),
            "tty%d",
            i
        );

        node->type = VFS_DEVICE;
        node->ops = &tty_vfs_ops;
        node->internal_data = devobj;

        devobj->node = node;

        vfs_add_child(dev, node);
    }
}

void tty_input_char(int id, char c) {
    tty_t *tty = &ttys[id];

    if ((tty->termios.c_iflag & ICRNL) && c == '\r')
        c = '\n';

    if (!(tty->termios.c_lflag & ICANON)) {

        if (tty->termios.c_lflag & ECHO) {
            terminal_putchar(c);
            debug_putcharn(c);
        }

        size_t next = (tty->cooked_head + 1) % TTY_BUF_SIZE;

        if (next != tty->cooked_tail) {
            tty->cooked_buf[tty->cooked_head] = c;
            tty->cooked_head = next;
        }

        tty_wake_reader(tty);
        return;
    }

    if (c == 127) {
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

        tty_wake_reader(tty);
    }
}

int tty_ioctl(unsigned long request, void* argp) {
    tty_t* tty = &ttys[active_tty];
    debugln("tty_ioctl req=%lx", request);
    if (!argp)
        return -1;

    switch (request) {

        case 0x5401:
            memcpy(
                argp,
                &tty->termios,
                sizeof(struct termios)
            );
            return 0;

        case 0x5402:
        case 0x5403:
        case 0x5404:
            memcpy(
                &tty->termios,
                argp,
                sizeof(struct termios)
            );
            return 0;

        case 0x5413: {
            uint16_t* ws =
                (uint16_t*)argp;

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

    vfs_file_t* file =
        kmalloc(sizeof(vfs_file_t));

    if (!file)
        return NULL;

    memset(file, 0, sizeof(vfs_file_t));

    file->node = tty_devices[id].node;
    file->flags = flags;
    file->pos = 0;

    return file;
}
