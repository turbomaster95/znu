#ifndef _USERLIBC_NETINET_IN_H
#define _USERLIBC_NETINET_IN_H

#include <stdint.h>

// Define socklen_t if it isn't defined in your sys/types.h yet
typedef uint32_t socklen_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

// Internet Protocol (IPv4) address structure
struct in_addr {
    in_addr_t s_addr; // 32-bit IPv4 address in Network Byte Order
};

// Structure describing an Internet socket address (IPv4)
struct sockaddr_in {
    uint16_t        sin_family; // AF_INET
    in_port_t       sin_port;   // Port number (Network Byte Order)
    struct in_addr  sin_addr;   // Internet address
    unsigned char   sin_zero[8]; // Padding to match generic sockaddr size
};

// Protocol numbers found in the IP header 'protocol' field
#define IPPROTO_IP   0   // Dummy for IP
#define IPPROTO_ICMP 1   // Internet Control Message Protocol
#define IPPROTO_TCP  6   // Transmission Control Protocol
#define IPPROTO_UDP  17  // User Datagram Protocol
#define IPPROTO_RAW  255 // Raw IP packets

// Address families
#define AF_UNSPEC 0
#define AF_INET   2     // IPv4 Internet protocols
#define AF_INET6  10    // IPv6 Internet protocols

// Socket types (often expected here or in sys/socket.h)
#define SOCK_STREAM 1   // Stream (connection-oriented) socket
#define SOCK_DGRAM  2   // Datagram (connectionless) socket
#define SOCK_RAW    3   // Raw protocol layer access

// Standard well-known IP addresses
#define INADDR_ANY       ((in_addr_t) 0x00000000) // Bind to all interfaces
#define INADDR_LOOPBACK  ((in_addr_t) 0x7F000001) // Localhost (127.0.0.1)

#endif // _USERLIBC_NETINET_IN_H
