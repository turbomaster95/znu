#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <termios.h>

void tty_init(void);
void tty_input_char(char c);
size_t tty_read(char *buf, size_t count);
long tty_write(const char *buf, size_t count);
int tty_ioctl(unsigned long request, void *argp);

#endif
