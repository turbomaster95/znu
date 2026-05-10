#include <termios.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TTY_BUF_SIZE 1024
extern void terminal_putchar(char c);

typedef struct tty {
    char cooked_buf[TTY_BUF_SIZE];
    size_t cooked_head;
    size_t cooked_tail;

    char line_buf[TTY_BUF_SIZE];
    size_t line_len;

    struct termios termios;
} tty_t;

tty_t kernel_tty;

void tty_init(void) {
    memset(&kernel_tty, 0, sizeof(kernel_tty));

    kernel_tty.termios.c_lflag =
        ICANON |
        ECHO |
        ISIG;

    kernel_tty.termios.c_iflag =
        ICRNL;

    kernel_tty.termios.c_oflag =
        OPOST;
}


void tty_input_char(char c) {
    tty_t *tty = &kernel_tty;

    if (c == '\r')
        c = '\n';

    // backspace
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

    // echo
    if (tty->termios.c_lflag & ECHO) {
        terminal_putchar(c);
    }

    tty->line_buf[tty->line_len++] = c;

    // canonical mode
    if (c == '\n') {
        for (size_t i = 0; i < tty->line_len; i++) {
            size_t next =
                (tty->cooked_head + 1) % TTY_BUF_SIZE;

            if (next == tty->cooked_tail)
                break;

            tty->cooked_buf[tty->cooked_head] =
                tty->line_buf[i];

            tty->cooked_head = next;
        }

        tty->line_len = 0;
    }
}

size_t tty_read(char *buf, size_t count) {
    tty_t *tty = &kernel_tty;

    size_t got = 0;

    while (got < count &&
           tty->cooked_head != tty->cooked_tail) {

        buf[got++] =
            tty->cooked_buf[tty->cooked_tail];

        tty->cooked_tail =
            (tty->cooked_tail + 1) % TTY_BUF_SIZE;
    }

    return got;
}
