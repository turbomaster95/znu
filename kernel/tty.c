#include <termios.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <proc.h>

#define TTY_BUF_SIZE 1024

extern void terminal_putchar(char c);

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

tty_t kernel_tty;

static int tty_buffer_empty(tty_t *tty) {
    return tty->cooked_head == tty->cooked_tail;
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

void tty_init(void) {
    memset(&kernel_tty, 0, sizeof(kernel_tty));

    kernel_tty.termios.c_lflag =
        ICANON |
        ECHO |
        ISIG;

    kernel_tty.termios.c_iflag = ICRNL;
    kernel_tty.termios.c_oflag = OPOST;
}

void tty_input_char(char c) {
    tty_t *tty = &kernel_tty;

    if (c == '\r')
        c = '\n';

    if (c == '\b' || c == 127) {
        if (tty->line_len > 0) {
            tty->line_len--;

            if (tty->termios.c_lflag & ECHO) {
                terminal_putchar('\b');
                terminal_putchar(' ');
                terminal_putchar('\b');
            }
        }
        return;
    }

    if (tty->termios.c_lflag & ECHO) {
        terminal_putchar(c);
    }

    if (tty->line_len >= TTY_BUF_SIZE) {
        tty->line_len = 0; // simple overflow protection
        return;
    }

    tty->line_buf[tty->line_len++] = c;

    if (c == '\n') {

        for (size_t i = 0; i < tty->line_len; i++) {

            size_t next = (tty->cooked_head + 1) % TTY_BUF_SIZE;

            /* buffer full → drop remaining input safely */
            if (next == tty->cooked_tail)
                break;

            tty->cooked_buf[tty->cooked_head] = tty->line_buf[i];
            tty->cooked_head = next;
        }

        tty->line_len = 0;

        tty_wake_readers(tty);
    }
}

size_t tty_read(char *buf, size_t count) {
    tty_t *tty = &kernel_tty;

    size_t got = 0;

    while (tty_buffer_empty(tty)) {

        /*
         * Minimal blocking:
         * put process to sleep until input arrives
         */
        if (current_process) {
            current_process->state = TASK_WAITING;
        }

        /* yield CPU */
        __asm__ volatile("sti; hlt; cli");
    }

    while (got < count &&
           tty->cooked_head != tty->cooked_tail) {

        buf[got++] = tty->cooked_buf[tty->cooked_tail];

        tty->cooked_tail =
            (tty->cooked_tail + 1) % TTY_BUF_SIZE;
    }

    return got;
}


long tty_write(const char *buf, size_t count) {
    for (size_t i = 0; i < count; i++)
        terminal_putchar(buf[i]);

    return count;
}

int tty_ioctl(unsigned long request, void *argp) {
    tty_t *tty = &kernel_tty;

    switch (request) {

        case 0x5401: /* TCGETS */
            if (!argp) return -1;

            memcpy(argp, &tty->termios, sizeof(struct termios));
            return 0;

        case 0x5402: /* TCSETS */
        case 0x5403:
        case 0x5404:
            if (!argp) return -1;

            memcpy(&tty->termios, argp, sizeof(struct termios));
            return 0;

        case 0x5413: { /* TIOCGWINSZ */
            uint16_t *ws = argp;

            ws[0] = 25;
            ws[1] = 80;
            ws[2] = 0;
            ws[3] = 0;

            return 0;
        }
    }

    return -1;
}
