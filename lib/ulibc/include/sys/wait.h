#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options, ...);

#define WNOHANG 1
#define WUNTRACED 2

#define WIFEXITED(s) 1
#define WEXITSTATUS(s) 0
#define WIFSTOPPED(s) 0
#define WSTOPSIG(s) 0
#define WTERMSIG(s) 0

#endif
