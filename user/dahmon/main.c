#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

void daemonize() {
    pid_t pid;

    // 1. Fork off the parent process
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

    // 2. Become the session leader
    if (setsid() < 0) exit(EXIT_FAILURE);

    // 3. Catch, ignore or handle signals
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // 4. Fork again to ensure we are not a session leader
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // 5. Set new file permissions
    umask(0);

    // 6. Change the working directory to root
    chdir("/");

    // 7. Close all open file descriptors
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }
}

int main() {
    daemonize();

    // The daemon is now running in the background!
    while (1) {
        // Do your background work here (e.g., logging, network listener)
        sleep(30); 
    }
    return 0;
}
