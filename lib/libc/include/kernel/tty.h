#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <vfs.h>
#include <proc.h>

#define MAX_TTYS 2
#define TTY_BUF_SIZE 4096

typedef struct tty {
    char cooked_buf[TTY_BUF_SIZE];

    size_t cooked_head;
    size_t cooked_tail;

    char line_buf[TTY_BUF_SIZE];
    size_t line_len;

    struct termios termios;

    process_t* waiting_reader;
} tty_t;

typedef struct tty_device {
    tty_t* tty;
    vfs_node_t* node;
} tty_device_t;

extern tty_device_t tty_devices[MAX_TTYS];
extern int active_tty;

void tty_init(void);

void tty_input_char(int id, char c);

size_t tty_read(tty_t* tty, char* buf, size_t count);

long tty_write(tty_t* tty, const char* buf, size_t count);

int tty_ioctl(unsigned long request, void* argp);

vfs_file_t* tty_open_file(int id, int flags);

#endif
