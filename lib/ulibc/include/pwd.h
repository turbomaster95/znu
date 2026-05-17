#ifndef _USERLIBC_PWD_H
#define _USERLIBC_PWD_H

#include <sys/types.h> // Assumes uid_t and gid_t are defined here (usually unsigned ints)

// The standard POSIX password structure matching /etc/passwd fields
struct passwd {
    char  *pw_name;   // Username string
    char  *pw_passwd; // Encrypted password or 'x' placeholder string
    uid_t  pw_uid;    // Numeric User ID
    gid_t  pw_gid;    // Numeric Primary Group ID
    char  *pw_gecos;  // Real name / comment field
    char  *pw_dir;    // Path to the user's home directory (e.g., "/root")
    char  *pw_shell;  // Path to the default login shell (e.g., "/bin/sh")
};

// Core function prototypes required for account database lookups
struct passwd *getpwuid(uid_t uid);
struct passwd *getpwnam(const char *name);

#endif // _USERLIBC_PWD_H
