#ifndef NETDB_H
#define NETDB_H

#include <netinet/in.h>

struct addrinfo {
    int              ai_flags;      // AI_PASSIVE, AI_CANONNAME, etc.
    int              ai_family;     // AF_INET, AF_INET6, AF_UNSPEC
    int              ai_socktype;   // SOCK_STREAM, SOCK_DGRAM
    int              ai_protocol;   // IPPROTO_TCP, IPPROTO_UDP
    socklen_t        ai_addrlen;    // Size of ai_addr
    struct sockaddr *ai_addr;       // Pointer to sockaddr structure
    char            *ai_canonname;  // Canonical name for host
    struct addrinfo *ai_next;       // Pointer to next item in linked list
};

struct hostent {
    char  *h_name;       // Official name of host
    char **h_aliases;    // Alias list
    int    h_addrtype;   // Host address type (e.g., AF_INET)
    int    h_length;     // Length of address
    char **h_addr_list;  // List of addresses from name server
};
#define h_addr h_addr_list[0] // Backward compatibility macro

struct servent {
    char  *s_name;     // Official name of service
    char **s_aliases;  // Alias list
    int    s_port;     // Port number (in network byte order)
    char  *s_proto;    // Protocol to use
};

#define AI_PASSIVE     0x0001
#define AI_CANONNAME   0x0002
#define AI_NUMERICHOST 0x0004
#define AI_NUMERICSERV 0x0008
#define AI_V4MAPPED    0x0010
#define AI_ALL         0x0020
#define AI_ADDRCONFIG  0x0040

#define EAI_BADFLAGS    -1
#define EAI_NONAME      -2
#define EAI_AGAIN       -3
#define EAI_FAIL        -4
#define EAI_FAMILY      -6
#define EAI_SOCKTYPE    -7
#define EAI_SERVICE     -8
#define EAI_MEMORY      -10
#define EAI_SYSTEM      -11

int getaddrinfo(const char *restrict node, const char *restrict service,
                const struct addrinfo *restrict hints,
                struct addrinfo **restrict res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int errcode);

struct hostent *gethostbyname(const char *name);
struct servent *getservbyname(const char *name, const char *proto);

#endif // NETDB_H
