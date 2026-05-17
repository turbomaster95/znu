#ifndef _USERLIBC_GRP_H
#define _USERLIBC_GRP_H

#include <sys/types.h> // Assumes gid_t is defined here

// The standard POSIX group structure
struct group {
    char   *gr_name;   // Group name string
    char   *gr_passwd; // Encrypted password or '*' placeholder
    gid_t   gr_gid;    // Numeric Group ID
    char  **gr_mem;    // Null-terminated array of pointers to member usernames
};

// Core function prototypes for group database lookups
struct group *getgrgid(gid_t gid);
struct group *getgrnam(const char *name);

#endif // _USERLIBC_GRP_H
