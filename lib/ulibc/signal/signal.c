#include <signal.h>

int sigemptyset(sigset_t *set) {
    if (set) *set = 0;
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    // For now, do nothing. Your kernel doesn't have a scheduler/signals yet.
    return 0;
}