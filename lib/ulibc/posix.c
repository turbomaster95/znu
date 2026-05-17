#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <syscall.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <dirent.h>
#include <termios.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>
#include <sys/select.h>

int errno = 0;
char **environ = NULL;

#define ATEXIT_MAX 32

static void (*atexit_funcs[ATEXIT_MAX])(void);
static int atexit_count = 0;

static inline uint64_t syscall0(uint64_t n) {
    uint64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall1(uint64_t n, uint64_t a1) {
    uint64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall2(uint64_t n, uint64_t a1, uint64_t a2) {
    uint64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

const char *const sys_siglist[NSIG] = {
    [0] = "Signal 0",
    [SIGHUP] = "Hangup",
    [SIGINT] = "Interrupt",
    [SIGQUIT] = "Quit",
    [SIGILL] = "Illegal instruction",
    [SIGTRAP] = "Trace/breakpoint trap",
    [SIGABRT] = "Aborted",
    [SIGKILL] = "Killed",
    [SIGSEGV] = "Segmentation fault",
    [SIGPIPE] = "Broken pipe",
    [SIGALRM] = "Alarm clock",
    [SIGTERM] = "Terminated",
    [SIGCHLD] = "Child exited",
    [SIGCONT] = "Continued",
    [SIGTSTP] = "Stopped (tty output)",
    [SIGTTIN] = "Stopped (tty input)",
    [SIGTTOU] = "Stopped",
};

ssize_t read(int fd, void* buf, size_t count) {
    ssize_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(0), "D"(fd), "S"(buf), "d"(count) : "rcx", "r11", "memory");
    return ret;
}

ssize_t write(int fd, const void* buf, size_t count) {
    ssize_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(1), "D"(fd), "S"(buf), "d"(count) : "rcx", "r11", "memory");
    return ret;
}

int close(int fd) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(3), "D"(fd) : "rcx", "r11", "memory");
    return ret;
}

int open(const char* pathname, int flags, ...) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(2), "D"(pathname), "S"(flags) : "rcx", "r11", "memory");
    return ret;
}

int execve(const char* filename, char* const argv[], char* const envp[]) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(59), "D"(filename), "S"(argv), "d"(envp) : "rcx", "r11", "memory");
    return ret;
}

int execv(const char *path, char *const argv[]) {
    char *env[] = { 0 };
    return execve(path, argv, env);
}

pid_t fork(void) {
    pid_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(57) : "rcx", "r11", "memory");
    return ret;
}

pid_t vfork(void) {
    return fork();
}

pid_t getpid(void) {
    ssize_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(39) : "rcx", "r11", "memory");
    return (pid_t)ret;
}

pid_t getppid(void) {
    return (pid_t)syscall0(110);
}

uid_t getuid(void) {
    return (uid_t)syscall0(102);
}

gid_t getgid(void) {
    return (gid_t)syscall0(104);
}

uid_t geteuid(void) {
    return (uid_t)syscall0(107);
}

gid_t getegid(void) {
    return (gid_t)syscall0(108);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return 0;
}

int kill(pid_t pid, int sig) {
    return 0;
}

int getgroups(int size, gid_t list[]) {
    return 0;
}

clock_t times(struct tms *buf) {
    return 0;
}

int atexit(void (*function)(void)) {
    if (atexit_count >= ATEXIT_MAX) {
        return -1; 
    }
    atexit_funcs[atexit_count++] = function;
    return 0;
}

void atexit_handle_exit(void) {
    while (atexit_count > 0) {
        atexit_funcs[--atexit_count]();
    }
}

int fstat(int fd, struct stat* statbuf) {
    return (int)syscall2(5, (uint64_t)fd, (uint64_t)statbuf);
}

int stat(const char* pathname, struct stat* statbuf) {
    return (int)syscall2(4, (uint64_t)pathname, (uint64_t)statbuf);
}

int lstat(const char* pathname, struct stat* statbuf) {
    return (int)syscall2(6, (uint64_t)pathname, (uint64_t)statbuf);
}

int isatty(int fd) {
    if (fd >= 0 && fd <= 2) return 1;
    return 0;
}

int dup(int oldfd) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(32), "D"(oldfd) : "rcx", "r11", "memory");
    return ret;
}

int dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(33), "D"(oldfd), "S"(newfd) : "rcx", "r11", "memory");
    return ret;
}

int pipe(int pipefd[2]) {
    return -1;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, 
           fd_set *exceptfds, struct timeval *timeout) {
    /* 
     * For now, we stub this to return 0 (timeout). 
     * This prevents the shell from hanging if it's in async mode.
     * In a real implementation, this would be a syscall:
     * return syscall(SYS_select, nfds, readfds, writefds, exceptfds, timeout);
     */
    return 0; 
}

void exit(int status) {
    atexit_handle_exit();
    __asm__ volatile ("syscall" : : "a"(60), "D"(status) : "rcx", "r11", "memory");
    while(1);
}

void _exit(int status) {
    exit(status);
}

void abort(void) {
    exit(1);
}

int raise(int sig) {
    return 0;
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

int munmap(void *addr, size_t length) {
    return 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return (void*)-1;
}

int sigfillset(sigset_t *set) {
    if (set) *set = (sigset_t)-1;
    return 0;
}

int sigaddset(sigset_t *set, int signum) {
    if (set) *set |= (1 << (signum - 1));
    return 0;
}

int sigdelset(sigset_t *set, int signum) {
    if (set) *set &= ~(1 << (signum - 1));
    return 0;
}

int sigismember(const sigset_t *set, int signum) {
    if (set) return (*set & (1 << (signum - 1))) != 0;
    return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    return 0;
}

int wait4(pid_t pid, int *status, int options, struct rusage *usage) {
    return (int)syscall2(61, (uint64_t)pid, (uint64_t)status);
}

pid_t wait(int *status) {
    return wait4(-1, status, 0, NULL);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    return wait4(pid, status, options, NULL);
}

pid_t wait3(int *status, int options, struct rusage *usage) {
    return wait4(-1, status, options, usage);
}

int access(const char *pathname, int mode) {
    return 0;
}

int chdir(const char *path) {
    return 0;
}

char *getcwd(char *buf, size_t size) {
    if (buf && size > 0) {
        strncpy(buf, "/", size);
    }
    return buf;
}



int fcntl(int fd, int cmd, ...) {
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    return 0;
}

int ioctl(int fd, unsigned long request, ...) {
    void* argp;

    __builtin_va_list ap;
    __builtin_va_start(ap, request);
    argp = __builtin_va_arg(ap, void*);
    __builtin_va_end(ap);

    return (int)sys_ioctl(fd, request, argp);
}

DIR *opendir(const char *name) {
    return NULL;
}

struct dirent *readdir(DIR *dirp) {
    return NULL;
}

int closedir(DIR *dirp) {
    return 0;
}

int iswblank(wint_t wc) {
    return wc == L' ' || wc == L'\t';
}

char *strerror(int errnum) {
    if (errnum == 0) return "Success";
    if (errnum == ENOENT) return "No such file or directory";
    if (errnum == EACCES) return "Permission denied";
    return "Unknown error";
}

int atoi(const char *nptr) {
    return (int)strtoll(nptr, NULL, 10);
}

int fchmod(int fd, mode_t mode) {
    return 0; 
}

sighandler_t signal(int signum, sighandler_t handler) {
    return NULL;
}

int tcgetattr(int fd, struct termios *termios_p) {
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)optional_actions;
    return ioctl(fd, TCSETS, (void*)termios_p);
}

pid_t tcgetpgrp(int fd) {
    return 0;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    return 0;
}

pid_t getpgrp(void) {
    return 0;
}

int setpgid(pid_t pid, pid_t pgid) {
    return 0;
}

mode_t umask(mode_t mask) {
    return 0;
}

uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xff000000) >> 24) |
           ((hostlong & 0x00ff0000) >> 8) |
           ((hostlong & 0x0000ff00) << 8) |
           ((hostlong & 0x000000ff) << 24);
}

char *setlocale(int category, const char *locale) {
    return "C";
}

size_t mbrlen(const char *s, size_t n, mbstate_t *ps) {
    if (s == NULL) return 0;
    return 1;
}

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    if (s == NULL) return 0;
    if (pwc) *pwc = (wchar_t)*s;
    return 1;
}

wchar_t *wcschr(const wchar_t *wcs, wchar_t wc) {
    while (*wcs) {
        if (*wcs == wc) return (wchar_t *)wcs;
        wcs++;
    }
    return NULL;
}

int iswspace(wint_t wc) {
    return wc == L' ' || wc == L'\t' || wc == L'\n' || wc == L'\r';
}

size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps) {
    size_t count = 0;
    const char *s = *src;
    while (*s && (!dest || count < len)) {
        if (dest) dest[count] = (wchar_t)*s;
        s++;
        count++;
    }
    if (dest && count < len) dest[count] = L'\0';
    *src = s;
    return count;
}

wctype_t wctype(const char *name) {
    return 0;
}

int iswctype(wint_t wc, wctype_t desc) {
    return 0;
}

char* getenv(const char* name) {
    if (!environ || !name) return NULL;
    size_t len = strlen(name);
    for (int i = 0; environ[i]; i++) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
            return &environ[i][len + 1];
        }
    }
    return NULL;
}

int mount(const char *source, const char *target, const char *fstype) {
    long ret = (long)syscall3(165, (uint64_t)source, (uint64_t)target, (uint64_t)fstype);
    
    if (ret < 0) {
        return -1;
    }
    
    return 0;
}

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    long ret = syscall3(318, (long)buf, (long)buflen, (long)flags);
    
    if (ret < 0) {
        return -1;
    }
    
    return (ssize_t)ret;
}

char *basename(char *path) {
    if (!path || !*path) {
        return ".";
    }

    char *p = path + strlen(path) - 1;

    // Strip trailing slashes
    while (p > path && *p == '/') {
        *p-- = '\0';
    }

    // Find the last remaining slash
    while (p >= path && *p != '/') {
        p--;
    }

    return (p < path) ? path : p + 1;
}

char *dirname(char *path) {
    if (!path || !*path) {
        return ".";
    }

    char *p = path + strlen(path) - 1;

    // Strip trailing slashes
    while (p > path && *p == '/') {
        *p-- = '\0';
    }

    // Find the last slash remaining
    while (p >= path && *p != '/') {
        p--;
    }

    if (p < path) {
        return ".";
    }

    if (p == path) {
        return "/";
    }

    *p = '\0';
    return path;
}
