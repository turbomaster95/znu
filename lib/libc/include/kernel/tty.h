#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <termios.h>
#include <vfs.h>
#include <proc.h>

#define TTY_BUF_SIZE 1024

typedef struct tty_waiter {
    process_t *proc;
    struct tty_waiter *next;
} tty_waiter_t;

typedef struct tty {
    char cooked_buf[TTY_BUF_SIZE];
    size_t cooked_head;
    size_t cooked_tail;

    char line_buf[TTY_BUF_SIZE];
    size_t line_len;

    struct termios termios;

    tty_waiter_t *readers;
} tty_t;

typedef struct tty_device {
    tty_t *tty;
    vfs_node_t *node;
} tty_device_t;

extern tty_device_t tty_devices[];
extern int active_tty;

void tty_init(void);
void tty_input_char(int id, char c);
size_t tty_read(char *buf, size_t count);
long tty_write(const char *buf, size_t count);
int tty_ioctl(unsigned long request, void *argp);
vfs_file_t* tty_open_file(int id, int flags);

#endif
