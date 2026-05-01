#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>
#include <stdint.h>

#define SIGHUP  1
#define SIGINT  2
#define SIGQUIT 3
#define SIGILL  4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGKILL 9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define NSIG 32
#define SIG_SETMASK 0
#define SIG_BLOCK   1
#define SIG_UNBLOCK 2
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIGERR ((void (*)(int))-1)

typedef void (*sighandler_t)(int);
typedef int sig_atomic_t;

sighandler_t signal(int signum, sighandler_t handler);
int kill(pid_t pid, int sig);
int sigemptyset(sigset_t *set);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int raise(int sig);
void (*signal(int sig, void (*func)(int)))(int);

#endif
