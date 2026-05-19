#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>

#define ZL_FREESTANDING
#define ZL_MALLOC malloc
#define ZL_FREE free
#define ZL_WRITE write
#define ZL_READ read

#include <zline.h>

#define EXEC    1
#define REDIR   2
#define PIPE    3
#define LIST    4
#define BACK    5
#define MAXARGS 10

struct cmd { int type; };
struct execcmd { int type; char *argv[MAXARGS]; char *eargv[MAXARGS]; };
struct redircmd { int type; struct cmd *cmd; char *file; int mode; int fd; };
struct pipecmd { int type; struct cmd *left; struct cmd *right; };
struct listcmd { int type; struct cmd *left; struct cmd *right; };
struct backcmd { int type; struct cmd *cmd; };

struct cmd *parsecmd(char*);

/* Attempts to execute the binary, falling back to /bin and /sbin paths */
void znu_exec(char *path, char **argv) {
    execv(path, argv);
    
    if (path[0] != '/') {
        char buf[256];
        snprintf(buf, sizeof(buf), "/bin/%s", path);
        execv(buf, argv);
        
        snprintf(buf, sizeof(buf), "/sbin/%s", path);
        execv(buf, argv);
    }
}

void runcmd(struct cmd *cmd) {
    struct execcmd *ecmd;
    struct redircmd *rcmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    int pfd[2];
    int pid, status;

    if (cmd == 0) exit(0);

    switch (cmd->type) {
    case EXEC:
        ecmd = (struct execcmd*)cmd;
        if (ecmd->argv[0] == 0) exit(1);
        znu_exec(ecmd->argv[0], ecmd->argv);
        printf("sh: command not found: %s\n", ecmd->argv[0]);
        exit(1);

    case REDIR:
        rcmd = (struct redircmd*)cmd;
        close(rcmd->fd); // Free up target slot (0 or 1)
        
        int file_fd = open(rcmd->file, rcmd->mode, 0644);
        if (file_fd < 0) {
            printf("sh: open %s failed\n", rcmd->file);
            exit(1);
        }
        // Since we closed rcmd->fd right before, open guarantees it snags that exact slot.
        // If your kernel doesn't guarantee lowest-available fd allocation, uncomment below:
        // dup2(file_fd, rcmd->fd); 

        runcmd(rcmd->cmd);
        break;

    case PIPE:
        pcmd = (struct pipecmd*)cmd;
        if (pipe(pfd) < 0) {
            printf("sh: pipe creation failed\n");
            exit(1);
        }

        /* 1. Fork out the left side stage process */
        if (fork() == 0) {
            close(1);       // Free stdout slot
            dup(pfd[1]);    // Duplicate write-end of pipe into stdout
            close(pfd[0]);  // Close unused read end
            close(pfd[1]);  // Close original write tracker
            runcmd(pcmd->left);
        }

        /* 2. Fork out the right side stage process */
        if (fork() == 0) {
            close(0);       // Free stdin slot
            dup(pfd[0]);    // Duplicate read-end of pipe into stdin
            close(pfd[0]);  // Close original read tracker
            close(pfd[1]);  // Close unused write end
            runcmd(pcmd->right);
        }

        /* 3. Parent shell closes its tracking ends and reaps both pipeline parts */
        close(pfd[0]);
        close(pfd[1]);
        wait(&status);
        wait(&status);
        break;

    case LIST:
        lcmd = (struct listcmd*)cmd;
        if (fork() == 0) runcmd(lcmd->left);
        wait(&status);
        if (fork() == 0) runcmd(lcmd->right);
        wait(&status);
        break;

    case BACK:
        if (fork() == 0) runcmd(((struct backcmd*)cmd)->cmd);
        break;
    }
    exit(0);
}

void shell_completion(const char *buf, zl_completions_t *lc) {
    static const char *cmds[] = { "ls", "cat", "reboot", "shutdown", "clear", "mem", "exit", "mount", NULL };
    for (int i = 0; cmds[i]; i++)
        if (zl_startswith(cmds[i], buf)) zl_add_completion(lc, cmds[i]);
}

int main(void) {
    zline_t *zl = zline_init("\033[1;34mznu\033[0m \033[1;36m/\033[0m > ");
    zline_set_completion_callback(zl, shell_completion);

    while (1) {
        char *input = zline_read(zl);
        if (!input) continue;

        while (*input == ' ') input++;
        if (*input == '\0') continue;

        if (strcmp(input, "exit") == 0) break;
        if (strcmp(input, "clear") == 0) { printf("\033[2J\033[H"); continue; }
        
        // Custom internal chdir so the shell process updates its state
        if (strncmp(input, "cd ", 3) == 0) {
            if (chdir(input + 3) < 0) printf("cannot cd %s\n", input + 3);
            continue;
        }

        struct cmd *c = parsecmd(input);
        
        /* This fork keeps the core shell alive! */
        if (fork() == 0) {
            runcmd(c);
        }
        
        int status;
        wait(&status); // Wait for the spawned pipeline layer to finish execution
    }
    return 0;
}

/* --- Keep all your token parsing functions down here --- */
struct cmd* execcmd(void) {
    struct execcmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;
    return (struct cmd*)cmd;
}

struct cmd* redircmd(struct cmd *subcmd, char *file, int mode, int fd) {
    struct redircmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->mode = mode;
    cmd->fd = fd;
    return (struct cmd*)cmd;
}

struct cmd* pipecmd(struct cmd *left, struct cmd *right) {
    struct pipecmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*)cmd;
}

struct cmd* listcmd(struct cmd *left, struct cmd *right) {
    struct listcmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*)cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq) {
    char *s = *ps;
    while(s < es && strchr(whitespace, *s)) s++;
    if(q) *q = s;
    int ret = *s;
    switch(*s){
    case 0: break;
    case '|': case '(': case ')': case ';': case '&': case '<': s++; break;
    case '>': s++; if(*s == '>'){ ret = '+'; s++; } break;
    default:
        ret = 'a';
        while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) s++;
        break;
    }
    if(eq) *eq = s;
    while(s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return ret;
}

int peek(char **ps, char *es, char *toks) {
    char *s = *ps;
    while(s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

struct cmd* parsecmd(char *s) {
    char *es = s + strlen(s);
    struct cmd *cmd = parseline(&s, es);
    return cmd;
}

struct cmd* parseline(char **ps, char *es) {
    struct cmd *cmd = parsepipe(ps, es);
    if(peek(ps, es, ";")){
        gettoken(ps, es, 0, 0);
        cmd = listcmd(cmd, parseline(ps, es));
    }
    return cmd;
}

struct cmd* parsepipe(char **ps, char *es) {
    struct cmd *cmd = parseexec(ps, es);
    if(peek(ps, es, "|")){
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    return cmd;
}

struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es) {
    int tok; char *q, *eq;
    while(peek(ps, es, "<>")){
        tok = gettoken(ps, es, 0, 0);
        gettoken(ps, es, &q, &eq);
        *eq = 0; 
        if(tok == '<') cmd = redircmd(cmd, q, O_RDONLY, 0);
        else cmd = redircmd(cmd, q, O_WRONLY|O_CREAT, 1);
    }
    return cmd;
}

struct cmd* parseexec(char **ps, char *es) {
    char *q, *eq;
    int tok, argc = 0;
    struct execcmd *cmd;
    struct cmd *ret = execcmd();
    cmd = (struct execcmd*)ret;

    ret = parseredirs(ret, ps, es);
    while(!peek(ps, es, "|)&;")){
        if((tok=gettoken(ps, es, &q, &eq)) == 0) break;
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    for(int i=0; i<argc; i++) *cmd->eargv[i] = 0;
    return ret;
}
