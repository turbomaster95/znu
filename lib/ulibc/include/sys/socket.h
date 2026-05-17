#ifndef _USERLIBC_SYS_SOCKET_H
#define _USERLIBC_SYS_SOCKET_H

#include <stdint.h>
#include <netinet/in.h> // Pulls in socklen_t and address families

// Generic socket address structure used by the system call layer
struct sockaddr {
    uint16_t sa_family;    // Address family (AF_INET, AF_UNIX, etc.)
    char     sa_data[14];  // Direct address data payload bytes
};

// Structure used by sendmsg() and recvmsg() for scatter/gather I/O
struct msghdr {
    void         *msg_name;       // Optional address
    socklen_t     msg_namelen;    // Size of address
    struct iovec *msg_iov;        // Scatter/gather array
    int           msg_iovlen;     // Number of elements in msg_iov
    void         *msg_control;    // Ancillary data buffer
    socklen_t     msg_controllen; // Ancillary data buffer len
    int           msg_flags;      // Flags on received message
};

// Protocol families (usually identical to address families)
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   1
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6

// Local UNIX domain socket identifier
#define AF_UNIX   1 
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_RDM       4  // Reliable Delivered Messages (Missing)
#define SOCK_SEQPACKET 5  // Sequential Packet Socket  (Missing)

// Core socket function prototypes for userlibc mapping
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);

#endif // _USERLIBC_SYS_SOCKET_H
