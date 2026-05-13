#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall.h>

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#define ZL_FREESTANDING
#define ZL_MALLOC malloc
#define ZL_FREE free
#define ZL_WRITE write
#define ZL_READ read

#include <zline.h>

static zline_t *zl;

void shell_completion(const char *buf, zl_completions_t *lc) {
    static const char *commands[] = {
        "help",
        "ls",
        "cat",
        "reboot",
        "shutdown",
        "clear",
        "mem",
        "echo",
        "exit",
	"mount",
        NULL
    };

    for (int i = 0; commands[i]; i++) {
        if (zl_startswith(commands[i], buf)) {
            zl_add_completion(lc, commands[i]);
        }
    }
}

void spawn_and_wait(const char *path, char **argv) {
    int pid = sys_spawn(path, argv, NULL);

    if (pid >= 0) {
        int status = 0;
        sys_wait(pid, &status);
    } else {
        printf("?: %s\n", path);
    }
}

void try_spawn_program(const char *line) {
    char *argv[] = { (char*)line, NULL };

    int pid = sys_spawn(line, argv, NULL);

    if (pid < 0) {
        char path[256];

        strcpy(path, "/bin/");
        strcat(path, line);

        char *b_argv[] = { path, NULL };

        pid = sys_spawn(path, b_argv, NULL);

        if (pid < 0) {
            strcpy(path, "/sbin/");
            strcat(path, line);

            char *s_argv[] = { path, NULL };

            pid = sys_spawn(path, s_argv, NULL);
        }
    }

    if (pid >= 0) {
        int status = 0;
        sys_wait(pid, &status);
    } else {
        printf("?: %s\n", line);
    }
}

void run_config() {
    int fd = sys_open("/etc/zinit.conf", 0);

    if (fd < 0)
        return;

    char buf[1024];

    int n = sys_read(fd, buf, sizeof(buf) - 1);

    if (n > 0) {
        buf[n] = '\0';

        char *line = buf;

        while (*line) {

            char *end = line;

            while (*end && *end != '\n')
                end++;

            bool has_next = (*end == '\n');

            *end = '\0';

            if (*line != '#' && *line != '\0') {

                printf("zInit: Spawning %s -i...\n", line);

                char *argv[] = {
                    line,
                    "-i",
                    NULL
                };

                int pid = sys_spawn(line, argv, NULL);

                int status = 0;

                int waited_pid = sys_wait(pid, &status);

                printf(
                    "[zInit] Process %d exited with status %d\n",
                    waited_pid,
                    status
                );
            }

            if (!has_next)
                break;

            line = end + 1;
        }
    }

    sys_close(fd);
}

void cmd_ls(const char *line) {
    char path[256];

    const char *input_path = line + 2;

    while (*input_path == ' ')
        input_path++;

    if (*input_path == '\0') {
        strcpy(path, "/");
    } else if (*input_path != '/') {
        path[0] = '/';
        strcpy(path + 1, input_path);
    } else {
        strcpy(path, input_path);
    }

    int fd = sys_open(path, 0);

    if (fd < 0) {
        printf("ls: could not open '%s'\n", path);
        return;
    }

    znu_dirent_t dents[16];

    int bytes_read =
        sys_getdents(fd, dents, sizeof(dents));

    if (bytes_read > 0) {

        int count =
            bytes_read / sizeof(znu_dirent_t);

        for (int i = 0; i < count; i++) {

            if (dents[i].type == 2) {
                printf(
                    "\033[1;34m%s\033[0m (dir)\n",
                    dents[i].name
                );
            } else {
                printf(
                    "\033[1;32m%s\033[0m [%d bytes]\n",
                    dents[i].name,
                    dents[i].size
                );
            }
        }
    }

    sys_close(fd);
}

void cmd_cat(const char *path) {
    int fd = sys_open(path, 0);

    if (fd < 0) {
        printf("Could not open %s\n", path);
        return;
    }

    char buf[512];

    int n;

    while ((n = sys_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }

    printf("\n");

    sys_close(fd);
}

void cmd_mem() {
    struct sysinfo info;

    if (sys_sysinfo(&info) != 0) {
        printf("sysinfo failed\n");
        return;
    }

    printf("Memory usage:\n");
    printf(
        "  Total: %ld MB\n",
        info.totalram / 1024 / 1024
    );

    printf(
        "  Free:  %ld MB\n",
        info.freeram / 1024 / 1024
    );

    printf(
        "  Used:  %ld MB\n",
        (info.totalram - info.freeram)
            / 1024 / 1024
    );

    printf(
        "  Procs: %d\n",
        info.procs
    );
}

static int split_args(char *line, char **argv, int max_args) {
    int argc = 0;

    while (*line && argc < max_args) {
        while (*line == ' ')
            line++;

        if (*line == '\0')
            break;

        argv[argc++] = line;

        while (*line && *line != ' ')
            line++;

        if (*line == ' ') {
            *line = '\0';
            line++;
        }
    }

    return argc;
}

static void cmd_mount(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: mount <device> <fstype> <mountpoint>\n");
        printf("example: mount /dev/ahci0 fat32 /mnt\n");
        return;
    }

    const char* device = argv[1];
    const char* fstype = argv[2];
    const char* path   = argv[3];

    printf("[shell] mounting %s (%s) -> %s\n",
        device, fstype, path);

    if (mount(device, path, fstype) == -22) {
        printf("[shell] mount failed\n");
        return;
    }

    printf("[shell] mount ok\n");
}

int main() {

    printf("Hello!\n");
    printf("---------------------------------\n");
    printf("      Starting zInit v0.1.0      \n");
    printf("---------------------------------\n");

    run_config();

    zl = zline_init(
        "\033[1;34mznu\033[0m "
        "\033[1;36m/\033[0m > "
    );

    zline_set_completion_callback(
        zl,
        shell_completion
    );

    while (1) {

        char *line = zline_read(zl);

        if (!line) {
            printf("^C\n");
            continue;
        }

        while (*line == ' ')
            line++;

        if (*line == '\0')
            continue;

        if (strcmp(line, "help") == 0) {

            printf(
                "Commands:\n"
                "  help\n"
                "  ls [dir]\n"
                "  cat <file>\n"
                "  reboot\n"
                "  shutdown\n"
                "  clear\n"
                "  mem\n"
                "  echo <text>\n"
                "  exit\n"
            );

        } else if (strcmp(line, "reboot") == 0) {

            sys_reboot();

        } else if (strcmp(line, "shutdown") == 0) {

            sys_shutdown();

        } else if (strcmp(line, "clear") == 0) {

            printf("\033[2J\033[H");

        } else if (strncmp(line, "ls", 2) == 0) {

            cmd_ls(line);

        } else if (strncmp(line, "cat ", 4) == 0) {

            cmd_cat(line + 4);

        } else if (strcmp(line, "mem") == 0) {

            cmd_mem();

        } else if (strncmp(line, "echo ", 5) == 0) {

            printf("%s\n", line + 5);

        } else if (strcmp(line, "exit") == 0) {

            sys_shutdown();
            sys_exit(0);

        } else if (strncmp(line, "mount", 5) == 0) {
	    
	    char *argv[8];
            int argc = split_args(line, argv, 8);
	    cmd_mount(argc, argv);

        } else {

            try_spawn_program(line);
        }
    }

    return 0;
}
