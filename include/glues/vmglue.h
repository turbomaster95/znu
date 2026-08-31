#ifndef VMGLUE_H
#define VMGLUE_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <page.h>
#include <vfse.h>

// Unsigned Integers
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Signed Integers
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

#define MAX_REGS 16
#define MAX_STACK_SIZE 512

#define ROM_SIZE 2048
#define RAM_SIZE 65536

#define VM_MAGIC 0x4D56535A // "ZSVM" in ASCII
#define VM_VERSION 2

// CPU Flags
#define FLAG_ZERO     (1 << 0)
#define FLAG_NEGATIVE (1 << 1)
#define FLAG_CARRY    (1 << 2)
#define FLAG_OVERFLOW (1 << 3)

// Glue Functions
#define FILESTRUCT vfse_file_t
#define READFILE(ptr, size, nmemb, stream)   vfse_read_file_wrapper((ptr), (size), (nmemb), (stream))
#define WRITFILE(ptr, size, nmemb, stream)   vfse_write_file_wrapper((ptr), (size), (nmemb), (stream))
#define OPENFILE(path, mode)                 vfse_fopen_wrapper((path), (mode))
#define CLOSFILE(stream)                     vfse_fclose_wrapper(stream)
#define PUTSFILE(str, stream)                vfse_fputs_wrapper((str), (stream))
#define GLUEMALLOC kmalloc
#define GLUEFREE   kfree
#define PRINTF     debugln

static inline int vfse_mode_to_flags(const char* mode) {
    if (!mode) return VFSE_O_RDONLY;
    if (strchr(mode, '+')) {
        if (mode[0] == 'r') return VFSE_O_RDWR;
        if (mode[0] == 'w') return VFSE_O_RDWR | VFSE_O_CREAT | VFSE_O_TRUNC;
        if (mode[0] == 'a') return VFSE_O_RDWR | VFSE_O_CREAT | VFSE_O_APPEND;
    }
    if (mode[0] == 'w') return VFSE_O_WRONLY | VFSE_O_CREAT | VFSE_O_TRUNC;
    if (mode[0] == 'a') return VFSE_O_WRONLY | VFSE_O_CREAT | VFSE_O_APPEND;
    return VFSE_O_RDONLY;
}

static inline FILESTRUCT* vfse_fopen_wrapper(const char* path, const char* mode) {
    int flags = vfse_mode_to_flags(mode);
    int fd = vfse_open(path, flags, 0644);
    if (fd < 0 || !vfse_current || fd >= VFSE_MAX_FDS) {
        return NULL;
    }
    return vfse_current->fds[fd];
}

static inline size_t vfse_read_file_wrapper(void* ptr, size_t size, size_t nmemb, FILESTRUCT* stream) {
    if (!stream || size == 0 || nmemb == 0) return 0;
    // Find matching fd for stream instance
    int target_fd = -1;
    for (int i = 0; i < VFSE_MAX_FDS; i++) {
        if (vfse_current->fds[i] == stream) {
            target_fd = i;
            break;
        }
    }
    if (target_fd < 0) return 0;
    
    ssize_t bytes_read = vfse_read(target_fd, ptr, size * nmemb);
    return (bytes_read > 0) ? (size_t)bytes_read / size : 0;
}

static inline size_t vfse_write_file_wrapper(const void* ptr, size_t size, size_t nmemb, FILESTRUCT* stream) {
    if (!stream || size == 0 || nmemb == 0) return 0;
    int target_fd = -1;
    for (int i = 0; i < VFSE_MAX_FDS; i++) {
        if (vfse_current->fds[i] == stream) {
            target_fd = i;
            break;
        }
    }
    if (target_fd < 0) return 0;

    ssize_t bytes_written = vfse_write(target_fd, ptr, size * nmemb);
    return (bytes_written > 0) ? (size_t)bytes_written / size : 0;
}

static inline int vfse_fclose_wrapper(FILESTRUCT* stream) {
    if (!stream) return -1;
    for (int i = 0; i < VFSE_MAX_FDS; i++) {
        if (vfse_current->fds[i] == stream) {
            return vfse_close(i);
        }
    }
    return -1;
}

static inline int vfse_fputs_wrapper(const char* str, FILESTRUCT* stream) {
    if (!str || !stream) return -1;
    size_t len = strlen(str);
    size_t written = vfse_write_file_wrapper(str, 1, len, stream);
    return (written == len) ? (int)written : -1;
}

#endif // VMGLUE_H
