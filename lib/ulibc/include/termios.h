#ifndef _TERMIOS_H
#define _TERMIOS_H

typedef unsigned char  cc_t;
typedef unsigned int   speed_t;
typedef unsigned int   tcflag_t;

#define NCCS 32
struct termios {
    tcflag_t c_iflag;    /* input mode flags */
    tcflag_t c_oflag;    /* output mode flags */
    tcflag_t c_cflag;    /* control mode flags */
    tcflag_t c_lflag;    /* local mode flags */
    cc_t c_line;         /* line discipline */
    cc_t c_cc[NCCS];     /* control characters */
};

#define TCGETS  0x5401
#define TCSETS  0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TCIFLUSH  0     // Flush data received but not read
#define TCOFLUSH  1     // Flush data written but not transmitted
#define TCIOFLUSH 2


/* c_iflag bits */
#define BRKINT  0000002
#define ICRNL   0000400
#define INPCK   0000020
#define ISTRIP  0000040
#define IXON    0002000

/* c_oflag bits */
#define OPOST   0000001
#define ONLCR 0000004

/* c_cflag bits */
#define CS8     0000060

/* c_lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define IEXTEN  0010000

/* c_cc characters */
#define VMIN    6
#define VTIME   5

#define TCSANOW    0
#define TCSADRAIN  1
#define TCSAFLUSH  2
#define ECHOE   0000020 
#define ECHOK   0000040 
#define ECHONL  0000100

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);

#endif
