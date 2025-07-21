/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
#define MAXCMDS 10

pid_t child_pid = -1;
sigjmp_buf jmpbuf;
pid_t pipeline_pids[MAXCMDS];
int pipeline_count = 0;

/* Function prototypes */
char *myshell_readinput(char *buf);
int myshell_parseinput(char *buf, char **argv);
void myshell_execute(char **argv, int bg, char *cmdline);
void try_execve(char **argv);
void myshell_execute_pipeline(char *cmdline);
void remove_quotes(char *s);
int parse_args_quote_aware(char *buf, char **argv);

void sigint_handler(int sig) {
    if (pipeline_count > 0){
        for (int i=0; i<pipeline_count; i++){
            if(pipeline_pids[i] > 0){
                kill(pipeline_pids[i], SIGINT);
            }
        }
        for (int i=0; i<pipeline_count; i++){
            if(pipeline_pids[i] > 0){
                waitpid(pipeline_pids[i], NULL, 0);
            }
        }
    }
    else if (child_pid > 0) {
        kill(child_pid, SIGINT);
        Waitpid(child_pid, NULL, 0);
        child_pid = -1;
    } 
    siglongjmp(jmpbuf, 1);
}

int main() 
{
    Signal(SIGINT, sigint_handler);

    char cmdline[MAXLINE]; /* Command line */
    char *argv[MAXARGS];
    int bg;

    while (1) {
	/* Read */
        if (sigsetjmp(jmpbuf, 1) == 1) {
            printf("\n");
        }

	    printf("CSE4100-SP-P2> ");
        fflush(stdout);
        
        if(myshell_readinput(cmdline) == NULL) continue;

        if(strchr(cmdline, '|')){
            myshell_execute_pipeline(cmdline);
            continue;
        }
        bg = myshell_parseinput(cmdline, argv);

        myshell_execute(argv, bg, cmdline);
    }
    return 0;
}
/* $end shellmain */

char *myshell_readinput(char *buf){
    if (fgets(buf, MAXLINE, stdin) == NULL) {
        if (feof(stdin)) exit(0);
        if (ferror(stdin)){
            if(errno == EINTR){
                clearerr(stdin);
            }
        }
        return NULL;
    }
    return buf;
}
  
/* $begin eval */
/* eval - Evaluate a command line */
void myshell_execute(char **argv, int bg, char *cmdline)
{
    pid_t pid;

    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    if (builtin_command(argv)) return;

    if ((pid = Fork()) == 0) { // 외부 명령어 -> 자식 프로세스
        Signal(SIGINT, SIG_DFL);
        if (strchr(argv[0], '/')){
            execve(argv[0], argv, environ);
            fprintf(stderr, "myshell: %s: %s\n", argv[0], strerror(errno));
            exit(127);
        } else{
            try_execve(argv);
        }
    } else {
	/* Parent waits for foreground job to terminate */
        child_pid = pid;
        if (!bg){ 
            int status;
            Waitpid(pid, &status, 0); // fg 실행의 경우, 자식 프로세스 종료 시까지 wait
            child_pid = -1;
        }
        else //when there is backgrount process!
            printf("%d %s", pid, cmdline);
    }
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "exit")) exit(0);  
    if (!strcmp(argv[0], "cd")){
        if(argv[1] != NULL){
            char *target = argv[1] ? argv[1] : getenv("HOME");
            if(chdir(target) != 0){ // 디렉토리 변경 실패
                perror("cd");
            }
        }
        return 1;
    }
    if (!strcmp(argv[0], "&")) return 1;
    return 0; // not a builtin command
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int myshell_parseinput(char *buf, char **argv) 
{
    int bg;              /* Background job? */

    buf[strlen(buf) - 1] = ' ';  // Replace trailing newline
    int argc = parse_args_quote_aware(buf, argv);

    if (argc == 0) return 1;

    for (int i = 0; i < argc; i++) {
        remove_quotes(argv[i]);  // 안전하게 사용 가능해짐
    }

    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

void try_execve(char **argv){
    char *path = getenv("PATH");
    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");

    while (dir != NULL){
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, argv[0]);

        if(access(fullpath, X_OK)==0){
            execve(fullpath, argv, environ);
            perror("execve");
            exit(127);
        }
        dir = strtok(NULL, ":");
    }
    fprintf(stderr, "Command '%s' not found\n", argv[0]);
    free(path_copy);
    exit(127);
}

void myshell_execute_pipeline(char *cmdline){
    char *trim_cmdline = cmdline;
    while (*trim_cmdline && isspace(*trim_cmdline)) trim_cmdline++;
    
    if (*trim_cmdline == '|') {
        fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
        return;
    }

    char *cmds[MAXCMDS];
    int n_cmds = 0;

    pipeline_count = 0;
    for (int i = 0; i < MAXCMDS; i++) {
        pipeline_pids[i] = -1;
    }
    char *token = strtok(cmdline, "|");
    while (token && n_cmds < MAXCMDS) {
        while (*token == ' ') token++; // 앞쪽 공백 제거
        cmds[n_cmds++] = strdup(token); // 명령어 저장
        token = strtok(NULL, "|");
    }

    for (int i = 0; i < n_cmds; ++i) {
        if (cmds[i] == NULL || strlen(cmds[i]) == 0) {
            fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
            return;
        }
    }

    int prev_fd = -1;

    for (int i = 0; i < n_cmds; ++i){
        int pipefd[2];
        if (i < n_cmds - 1) {
            if(pipe(pipefd) < 0){
                perror("pipe");
                for(int j = 0; j < pipeline_count; j++){
                    if (pipeline_pids[j] > 0) {
                        kill(pipeline_pids[j], SIGKILL);
                        waitpid(pipeline_pids[j], NULL, 0);
                    }
                }
                pipeline_count = 0;
                for (int j = 0; j < n_cmds; j++) free(cmds[j]);
                return;
            }
        }

        pid_t pid = fork();
        if (pid < 0){
            perror("fork");
            // 포크 실패 시 정리
            if (i < n_cmds - 1) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            for (int j = 0; j < pipeline_count; j++) {
                if (pipeline_pids[j] > 0) {
                    kill(pipeline_pids[j], SIGKILL);
                    waitpid(pipeline_pids[j], NULL, 0);
                }
            }
            pipeline_count = 0;
            for (int j = 0; j < n_cmds; j++) {
                free(cmds[j]);
            }
            return;
        }
        if(pid == 0){ // 자식 프로세스
            Signal(SIGINT, SIG_DFL);

            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO); // 앞 명령어의 출력을 내 입력으로
                close(prev_fd);
            }
            if (i<n_cmds -1){
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            char *local_argv[MAXARGS];
            myshell_parseinput(cmds[i], local_argv);
            if(local_argv[0] == NULL) exit(0);

            if (builtin_command(local_argv)) {
                exit(0);
            }

            if (strchr(local_argv[0], '/')) {
                execve(local_argv[0], local_argv, environ);
                fprintf(stderr, "myshell: %s: %s\n", local_argv[0], strerror(errno));
                exit(127);
            } else {
                try_execve(local_argv);
            }
            exit(127);
        }
        if (pipeline_count < MAXCMDS) {
            pipeline_pids[pipeline_count++] = pid;
        }
        // 자식은 부모의 복제본이므로 양쪽 모두에서 단자를 닫아줘야 함
        if (prev_fd != -1) close(prev_fd);
        if (i< n_cmds - 1){
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    // 자식 종료 대기
    int status;
    for (int i = 0; i < pipeline_count; ++i){
        if (pipeline_pids[i] > 0) {
            if (waitpid(pipeline_pids[i], &status, 0) < 0) {
                perror("waitpid");
            }
        }
    }
    pipeline_count = 0;
    for (int i = 0; i < n_cmds; ++i) free(cmds[i]);
}

void remove_quotes(char *s) {
    char *read = s, *write = s;
    while (*read) {
        if (*read != '"' && *read != '\'') {
            *write++ = *read;
        }
        read++;
    }
    *write = '\0';
}

int parse_args_quote_aware(char *buf, char **argv) {
    int argc = 0;
    char *p = buf;

    while (*p) {
        // skip leading spaces
        while (*p && isspace(*p)) p++;

        if (*p == '\0') break;

        if (*p == '\'' || *p == '"') {
            // quoted argument
            char quote = *p++;
            argv[argc++] = p;
            while (*p && *p != quote) p++;
            if (*p) *p++ = '\0';  // null-terminate argument
        } else {
            argv[argc++] = p;
            while (*p && !isspace(*p)) {
                if (*p == '"' || *p == '\'') {
                    // skip over quoted part
                    char quote = *p++;
                    while (*p && *p != quote) p++;
                    if (*p) p++;
                } else {
                    p++;
                }
            }
            if (*p) *p++ = '\0';
        }
    }

    argv[argc] = NULL;
    return argc;
}
