#ifdef __linux__
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;

#define sys_spawn(path, argv, env) ({ \
    pid_t _pid; \
    int _res = posix_spawn(&_pid, path, NULL, NULL, argv, environ); \
    (_res == 0) ? (int)_pid : -1; \
})

#define sys_wait(pid, status) waitpid(pid, status, 0)
#define sys_open  open
#define sys_read  read
#define sys_close close
#define sys_exit  exit
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define ZL_FREESTANDING
#define ZL_MALLOC malloc
#define ZL_FREE free
#define ZL_WRITE write
#define ZL_READ read

#include <zline.h>

#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5
#define MAXARGS 10

struct cmd { int type; };
struct execcmd { int type; char *argv[MAXARGS]; char *eargv[MAXARGS]; };
struct redircmd { int type; struct cmd *cmd; char *file; int mode; int fd; };
struct pipecmd { int type; struct cmd *left; struct cmd *right; };
struct listcmd { int type; struct cmd *left; struct cmd *right; };
struct backcmd { int type; struct cmd *cmd; };

struct cmd *parsecmd(char*);

int znu_spawn(char *path, char **argv) {
    int pid = sys_spawn(path, argv, NULL);
    if (pid < 0 && path[0] != '/') {
        char buf[256];
        snprintf(buf, sizeof(buf), "/bin/%s", path);
        pid = sys_spawn(buf, argv, NULL);
        if (pid < 0) {
            snprintf(buf, sizeof(buf), "/sbin/%s", path);
            pid = sys_spawn(buf, argv, NULL);
        }
    }
    return pid;
}

void runcmd(struct cmd *cmd) {
    struct execcmd *ecmd;
    struct redircmd *rcmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    int pid, status;

    if(cmd == 0) exit(0);

    switch(cmd->type){
    case EXEC:
        ecmd = (struct execcmd*)cmd;
        if(ecmd->argv[0] == 0) exit(1);
        
        pid = znu_spawn(ecmd->argv[0], ecmd->argv);
        if (pid >= 0) sys_wait(pid, &status);
        else printf("sh: command not found: %s\n", ecmd->argv[0]);
        break;

    case REDIR:
        rcmd = (struct redircmd*)cmd;
        runcmd(rcmd->cmd);
        break;

    case LIST:
        lcmd = (struct listcmd*)cmd;
        runcmd(lcmd->left);
        runcmd(lcmd->right);
        break;

    case PIPE:
        pcmd = (struct pipecmd*)cmd;
        runcmd(pcmd->left);
        runcmd(pcmd->right);
        break;

    case BACK:
        runcmd(((struct backcmd*)cmd)->cmd);
        break;
    }
}

void shell_completion(const char *buf, zl_completions_t *lc) {
    static const char *cmds[] = { "ls", "cat", "reboot", "shutdown", "clear", "mem", "exit", "mount", NULL };
    for (int i = 0; cmds[i]; i++)
        if (zl_startswith(cmds[i], buf)) zl_add_completion(lc, cmds[i]);
}

int main(void) {
    zline_t *zl = zline_init("\033[1;34mznu\033[0m \033[1;36m/\033[0m > ");
    zline_set_completion_callback(zl, shell_completion);

    while(1) {
        char *input = zline_read(zl);
        if(!input) continue;

        while(*input == ' ') input++;
        if(*input == '\0') continue;

        if(strcmp(input, "exit") == 0) break;
        if(strcmp(input, "clear") == 0) { printf("\033[2J\033[H"); continue; }
        if(strncmp(input, "cd ", 3) == 0) {
            if(chdir(input+3) < 0) printf("cannot cd %s\n", input+3);
            continue;
        }

        struct cmd *c = parsecmd(input);
        runcmd(c);
    }
    return 0;
}


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
        *eq = 0; // Null terminate filename
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
