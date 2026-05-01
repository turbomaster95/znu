#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <syscall.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <termios.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <errno.h>

int errno = 0;

ssize_t read(int fd, void* buf, size_t count) {
    return sys_read(fd, buf, count);
}

ssize_t write(int fd, const void* buf, size_t count) {
    return sys_write(fd, buf, count);
}

int close(int fd) {
    return sys_close(fd);
}

int open(const char* pathname, int flags, ...) {
    return sys_open(pathname, flags);
}

int raise(int sig) {
    return 0; 
}

void abort(void) {
    exit(1);
}

int pipe(int pipefd[2]) {
    errno = ENOSYS;
    return -1;
}

int dup2(int oldfd, int newfd) {
    errno = ENOSYS;
    return -1;
}

int optind = 1, opterr = 1, optopt = 0;
char *optarg = NULL;

int getopt(int argc, char * const argv[], const char *optstring) {
    return -1;
}

pid_t getpgrp(void) { return 0; }
int setpgid(pid_t pid, pid_t pgid) { return 0; }
pid_t tcgetpgrp(int fd) { return 0; }
int tcsetpgrp(int fd, pid_t pgrp) { return 0; }

pid_t fork(void) {
    pid_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(31) : "rcx", "r11", "memory"); // Syscall 31 is fork
    return ret;
}

pid_t vfork(void) {
    return fork();
}

int sigsuspend(const sigset_t *mask) { return 0; }

char *stpncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    char *ret = dest + i;
    for (; i < n; i++)
        dest[i] = '\0';
    return ret;
}

char *strsignal(int sig) {
    return "Unknown signal";
}

pid_t getpid(void) {
    pid_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(39) : "rcx", "r11", "memory");
    return ret;
}

int isatty(int fd) {
    if (fd >= 0 && fd <= 2) return 1;
    return 0;
}

int atoi(const char *nptr) {
    return (int)strtoll(nptr, NULL, 10);
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }

int stat64(const char* pathname, struct stat* statbuf) { return stat(pathname, statbuf); }
int lstat64(const char* pathname, struct stat* statbuf) { return lstat(pathname, statbuf); }

int lstat(const char *pathname, struct stat *statbuf) {
    return stat(pathname, statbuf);
}
// Wide char stubs
size_t mbrlen(const char *s, size_t n, mbstate_t *ps) { return 0; }
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) { return 0; }
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps) { return 0; }
wchar_t *wcschr(const wchar_t *s, wchar_t c) { return NULL; }
int iswspace(wint_t wc) { return 0; }
int iswctype(wint_t wc, wctype_t desc) { return 0; }
wctype_t wctype(const char *name) { return 0; }

char* getenv(const char* name) {
    // Minimal getenv: always return NULL for now
    return NULL;
}

int fcntl(int fd, int cmd, ...) {
    return 0;
}

int tcgetattr(int fd, struct termios *termios_p) {
    memset(termios_p, 0, sizeof(struct termios));
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    return 0;
}

pid_t wait(int *stat_loc) {
    return sys_wait(-1);
}

pid_t waitpid(pid_t pid, int *stat_loc, int options, ...) {
    return sys_wait(pid);
}

int ioctl(int fd, unsigned long request, ...) {
    return 0;
}

void exit(int status) {
    sys_exit(status);
    while(1);
}

void _exit(int status) {
    exit(status);
}

int fstat(int fd, struct stat* statbuf) {
    // Stub
    memset(statbuf, 0, sizeof(struct stat));
    return 0;
}

int stat(const char* pathname, struct stat* statbuf) {
    // Stub
    memset(statbuf, 0, sizeof(struct stat));
    return 0;
}

DIR *opendir(const char *name) {
    int fd = open(name, 0);
    if (fd < 0) return NULL;
    DIR *dir = malloc(sizeof(DIR));
    dir->fd = fd;
    dir->pos = 0;
    return dir;
}

struct dirent *readdir(DIR *dirp) {
    static struct dirent de;
    znu_dirent_t zd;
    int n = sys_getdents(dirp->fd, &zd, sizeof(zd));
    if (n <= 0) return NULL;
    de.d_ino = 0;
    de.d_type = (zd.type == 2) ? DT_DIR : DT_REG;
    strncpy(de.d_name, zd.name, 255);
    return &de;
}

int closedir(DIR *dirp) {
    close(dirp->fd);
    free(dirp);
    return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    return 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    void* ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(9), "D"(addr), "S"(length), "d"(prot), "r"(flags), "r"(fd) : "rcx", "r11", "memory");
    return ret;
}

int munmap(void *addr, size_t length) {
    return 0;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    // Simple implementation: just use printf for now
    int ret = printf(format, ap);
    va_end(ap);
    return ret;
}

int fputs(const char* s, FILE* stream) {
    size_t len = strlen(s);
    return write((int)(uintptr_t)stream, s, len);
}
