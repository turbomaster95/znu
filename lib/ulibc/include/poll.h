#ifndef _USERLIBC_POLL_H
#define _USERLIBC_POLL_H

// The structural format BusyBox passes to monitor event streams
struct pollfd {
    int   fd;       // The file descriptor to monitor
    short events;   // Requested event bitmask (input flags)
    short revents;  // Returned event bitmask (output filled by kernel)
};

// Typedef for counting poll descriptors cleanly
typedef unsigned long nfds_t;

// Event Bitmask Flags 
#define POLLIN     0x0001  // There is data to read
#define POLLPRI    0x0002  // There is urgent data to read (e.g., out-of-band)
#define POLLOUT    0x0004  // Writing now will not block

// Error / Status Flags returned in revents (ignored in events field)
#define POLLERR    0x0008  // Error condition occurred on the descriptor
#define POLLHUP    0x0010  // Hang up occurred (e.g., pipe channel closed)
#define POLLNVAL   0x0020  // Invalid request: fd is not open

// Function prototype for your userlibc linkage
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif // _USERLIBC_POLL_H
