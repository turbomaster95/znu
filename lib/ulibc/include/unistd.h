#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <signal.h> // For sigset_t
#include <sys/types.h>

#define GRND_NONBLOCK 0x0001
#define GRND_RANDOM   0x0002

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int close(int fd);
int execve(const char* filename, char* const argv[], char* const envp[]);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int getgroups(int size, gid_t list[]);

pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
int isatty(int fd);
int pipe(int pipefd[2]);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int dup(int oldfd);
int dup2(int oldfd, int newfd);
pid_t getpgrp(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
pid_t vfork(void);
int sigsuspend(const sigset_t *mask);
off_t lseek(int fd, off_t offset, int whence);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int atexit(void (*function)(void));
int mount(const char *source, const char *target, const char *fstype);
void _exit(int status);
int execv(const char *path, char *const argv[]);
int wait(int *status);
int access(const char *pathname, int mode);
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);
int unlink(const char *pathname);
int chown(const char *pathname, uid_t owner, gid_t group);
int symlink(const char *target, const char *linkpath);
int link(const char *oldpath, const char *newpath);
unsigned int alarm(unsigned int seconds);
unsigned int sleep(unsigned int seconds);

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#endif
