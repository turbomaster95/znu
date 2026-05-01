#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef int64_t ssize_t;
typedef int32_t pid_t;

typedef int64_t off_t;
typedef uint32_t mode_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint64_t dev_t;   // Device ID
typedef uint64_t ino_t;   // Inode number
typedef uint32_t mode_t;  // File mode (permissions)
typedef uint32_t uid_t;   // User ID
typedef uint32_t gid_t;   // Group ID
typedef int64_t  off_t;   // File offset

typedef uint64_t sigset_t;

typedef long time_t;
typedef long clock_t;

#endif