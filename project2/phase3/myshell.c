/* $begin shellmain */
#include "csapp.h"
#include <termios.h>
#define MAXARGS   128
#define MAXCMDS 10
#define MAXJOBS 128
#define MAXDONE 128

pid_t child_pid = -1;
sigjmp_buf jmpbuf;
pid_t pipeline_pids[MAXCMDS];
int pipeline_count = 0;

typedef enum { RUNNING, STOPPED } job_state;

typedef struct {
    int job_id;
    pid_t pgid;
    char cmdline[MAXLINE];
    job_state state;
} job_t;

typedef struct {
    int job_id;
    char cmdline[MAXLINE];
} done_job_t;

job_t jobs[MAXJOBS];
int next_job_id = 1;
int current_job = 0;
int previous_job = 0;

done_job_t done_jobs[MAXDONE];
int done_job_count = 0;

/* Function prototypes */
char *myshell_readinput(char *buf);
int myshell_parseinput(char *buf, char **argv);
void myshell_execute(char **argv, int bg, char *cmdline);
void try_execve(char **argv);
void myshell_execute_pipeline(char *cmdline);
void remove_quotes(char *s);
int parse_args_quote_aware(char *buf, char **argv);
int builtin_command(char **argv);

// job 관리 함수
int add_job(pid_t pgid, const char *cmdline, job_state state);
void delete_job(pid_t pgid);
job_t *find_job_by_id(int job_id);
job_t *find_job_by_pgid(pid_t pgid);
void list_jobs();

// fg, bg 실행 함수
void do_fg(int job_id);
void do_bg(int job_id);
void do_kill(int job_id);

// signal handler
void sigint_handler(int sig) {
    pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);
    pid_t shell_pgid = getpgrp();

    if (fg_pgid != shell_pgid) {
        Kill(-fg_pgid, SIGINT);
        write(STDOUT_FILENO, "\n", 1);
    }
    siglongjmp(jmpbuf, 1);
}

void sigtstp_handler(int sig) {
    pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);
    pid_t shell_pgid = getpgrp();

    if (fg_pgid != shell_pgid) {
        // 포그라운드 프로세스 그룹이 있는 경우
        Kill(-fg_pgid, SIGTSTP);
        job_t *job = find_job_by_pgid(fg_pgid);
        if (job) {
            job->state = STOPPED;
            printf("\n[%d]+  Stopped                 %s", 
                job->job_id, job->cmdline);
            if (job->cmdline[strlen(job->cmdline)-1] != '\n')
                printf("\n");
        }
        // 쉘로 제어권 돌려놓기
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }
    siglongjmp(jmpbuf, 1);
}


void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    int saved_errno = errno;  // errno 저장

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = find_job_by_pgid(pid);
        
        if (!job) {
            // pipeline의 자식 프로세스인 경우
            int is_pipeline_child = 0;
            for (int i = 0; i < pipeline_count; i++) {
                if (pipeline_pids[i] == pid) {
                    is_pipeline_child = 1;
                    break;
                }
            }
            if (is_pipeline_child) continue;

            // 그룹 리더가 아닌 프로세스인 경우, 그룹 ID로 다시 찾기 시도
            pid_t pgid;
            if ((pgid = getpgid(pid)) > 0 && pgid != pid) {
                job = find_job_by_pgid(pgid);
                if (job) continue; // 그룹 리더로 관리 중인 job 발견, 추가 작업 필요 없음
            }

            // 종료된 프로세스의 job이 없다면 생성하지 않고 넘어감
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                continue;
            }

            // 중지된 프로세스에 대해서만 Unknown job 생성
            if (WIFSTOPPED(status)) {
                char placeholder[MAXLINE];
                snprintf(placeholder, MAXLINE, "Unknown command (pid %d)", pid);
                add_job(pid, placeholder, STOPPED);
                job = find_job_by_pgid(pid);
            }
        }

        if (!job) continue;  // 잘못된 프로세스 건너뛰기

        if (WIFSTOPPED(status)) {
            // SIGTSTP로 중지된 경우
            job->state = STOPPED;
            
            // SIGTSTP_handler에서 메시지를 출력하므로 여기서는 다른 프로세스 중지 경우에만 출력
            pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);
            if (fg_pgid != job->pgid) {
                printf("\n[%d]+  Stopped                 %s",
                    job->job_id, job->cmdline);
                if (job->cmdline[strlen(job->cmdline)-1] != '\n') printf("\n");
            }
        } else if (WIFCONTINUED(status)) {
            job->state = RUNNING;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // 프로세스가 종료된 경우 done_jobs에 추가
            if (done_job_count < MAXDONE) {
                done_jobs[done_job_count].job_id = job->job_id;
                strncpy(done_jobs[done_job_count].cmdline, job->cmdline, MAXLINE - 1);
                done_jobs[done_job_count].cmdline[MAXLINE - 1] = '\0';
                done_job_count++;
            }
            delete_job(job->pgid);
        }
    }

    errno = saved_errno;  // errno 복원
}


int main() 
{
    pid_t shell_pid = getpid();
    setpgid(shell_pid, shell_pid);
    tcsetpgrp(STDIN_FILENO, shell_pid);
    
    // 시그널 핸들러 설정
    struct termios shell_tmodes;
    tcgetattr(STDIN_FILENO, &shell_tmodes);

    sigset_t mask_all, prev_all;
    sigfillset(&mask_all);


    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    Signal(SIGTSTP, sigtstp_handler);
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGTTOU, SIG_IGN);

    char cmdline[MAXLINE]; /* Command line */
    char cmdline_copy[MAXLINE];
    char *argv[MAXARGS];
    int bg;

    while (1) {
	/* Read */
        if (sigsetjmp(jmpbuf, 1) == 1) {
            printf("\n");
        }

        for (int i = 0; i < done_job_count; i++) {
            printf("[%d]   Done                    %s", 
                   done_jobs[i].job_id, 
                   done_jobs[i].cmdline);
            if (done_jobs[i].cmdline[strlen(done_jobs[i].cmdline) - 1] != '\n')
                printf("\n");
        }
        done_job_count = 0;

	    printf("CSE4100-SP-P2> ");
        fflush(stdout);
        
        if(myshell_readinput(cmdline) == NULL) continue;
        strcpy(cmdline_copy, cmdline);

        if(strchr(cmdline, '|')){
            myshell_execute_pipeline(cmdline_copy);
            continue;
        }
        bg = myshell_parseinput(cmdline, argv);

        myshell_execute(argv, bg, cmdline_copy);
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
    sigset_t prev_all;

    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    if (builtin_command(argv)) return;

    if ((pid = Fork()) == 0) { // 외부 명령어 -> 자식 프로세스
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        setpgid(0, 0);
        
        Signal(SIGINT, SIG_DFL);
        Signal(SIGTSTP, SIG_DFL);
        Signal(SIGCHLD, SIG_DFL);
        Signal(SIGTTOU, SIG_DFL);
    
        if (strchr(argv[0], '/')){
            execve(argv[0], argv, environ);
            fprintf(stderr, "myshell: %s: %s\n", argv[0], strerror(errno));
            exit(127);
        } else{
            try_execve(argv);
        }
    } else {
        setpgid(pid, pid);
        int job_id = add_job(pid, cmdline, RUNNING); // 모든 작업 추적을 위해 foreground도 jobs에 추가
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        if (!bg){ 
            tcsetpgrp(STDIN_FILENO, pid);

            int status;
            // 프로세스 그룹 전체 대기 (-pid)
            if (waitpid(-pid, &status, WUNTRACED) < 0) {
                if (errno != EINTR) {
                    perror("waitpid error");
                }
            }
            tcsetpgrp(STDIN_FILENO, getpgrp());
            
            // Ctrl+Z로 중지된 경우 job에서 삭제하지 않음
            if (WIFSTOPPED(status)) {
                // 여기서 상태를 명시적으로 STOPPED로 설정
                job_t *job = find_job_by_pgid(pid);
                if (job) {
                    job->state = STOPPED;
                }
            } else {
                // 정상 종료 또는 신호로 종료된 경우
                delete_job(pid);
            }
        }
        else // bg 실행 경우
        {
            printf("[%d] %d\n", job_id, pid);
        }
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
    if (!strcmp(argv[0], "jobs")) {
        list_jobs();
        return 1;
    }
    if (!strcmp(argv[0], "fg")) {
        int job_id = 0;
        if (argv[1]) {
            char *arg = argv[1];
            if (arg[0] == '%') arg++;
            job_id = atoi(arg);
        }
        else job_id = current_job;

        if (job_id > 0) do_fg(job_id);
        else fprintf(stderr, "myshell: fg: current: no such job\n");
        return 1;
    }
    if (!strcmp(argv[0], "bg")) {
        int job_id = 0;
        if (argv[1]) {
            char *arg = argv[1];
            if (arg[0] == '%') arg++;
            job_id = atoi(arg);
        }
        else job_id = current_job;

        if (job_id > 0)
            do_bg(job_id);
        else fprintf(stderr, "myshell: bg: current no such job\n");
        return 1;
    }
    if (!strcmp(argv[0], "kill")) {
        if (argv[1]) {
            char *arg = argv[1];
            if (arg[0] == '%') arg++;
            int job_id = atoi(arg);
            if (job_id > 0)
                do_kill(job_id);
            else
                fprintf(stderr, "myshell: kill: (%c) - Operation not permitted\n", argv[1]);
        }
        else fprintf(stderr, "kill: usage: kill [-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill -l [sigspec]\n");
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

    bg = 0;
    if (strcmp(argv[argc - 1], "&") == 0){
        bg = 1;
        argv[--argc] = NULL;
    }
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

    // 백그라운드 실행 여부 확인
    int bg = 0;
    size_t cmdlen = strlen(cmdline);
    if (cmdlen > 0) {
        char *end = cmdline + cmdlen - 1;
        while (end > cmdline && isspace(*end)) end--;
        if (*end == '&') {
            bg = 1;
            *end = ' '; // & 제거
        }
    }

    char *cmds[MAXCMDS];
    int n_cmds = 0;

    pipeline_count = 0;
    for (int i = 0; i < MAXCMDS; i++) {
        pipeline_pids[i] = -1;
    }
    char *token = strtok(cmdline, "|");
    while (token && n_cmds < MAXCMDS) {
        while (*token && isspace(*token)) token++; // 앞쪽 공백 제거
        if (*token) { // 빈 명령어 아닐 경우만 저장
            cmds[n_cmds++] = strdup(token); // 명령어 저장
        }
        token = strtok(NULL, "|");
    }

    if (n_cmds == 0) {
        fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
        return;
    }

    for (int i = 0; i < n_cmds; ++i) {
        if (cmds[i] == NULL || strlen(cmds[i]) == 0) {
            fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
            for (int j = 0; j < n_cmds; j++) {
                if (cmds[j]) free(cmds[j]);
            }
            return;
        }
    }

    int prev_fd = -1;
    pid_t pgid = -1;

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
                if (cmds[j]) free(cmds[j]);
            }
            return;
        }
        if(pid == 0){ // 자식 프로세스
            // 시그널 핸들러 복원
            Signal(SIGINT, SIG_DFL);
            Signal(SIGTSTP, SIG_DFL);
            Signal(SIGCHLD, SIG_DFL);
            Signal(SIGTTOU, SIG_DFL);

            // 파이프라인의 첫 번째 프로세스가 pgid가 됨
            if (i == 0) {
                setpgid(0, 0);
            } else {
                setpgid(0, pgid);
            }

            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO); // 앞 명령어의 출력을 내 입력으로
                close(prev_fd);
            }
            if (i < n_cmds - 1){
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

        // 파이프라인 프로세스 그룹 ID 설정
        if (i == 0) {
            pgid = pid;
        }
        setpgid(pid, pgid);

        if (pipeline_count < MAXCMDS) {
            pipeline_pids[pipeline_count++] = pid;
        }
        
        // 파이프 닫기
        if (prev_fd != -1) close(prev_fd);
        if (i < n_cmds - 1){
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    // 원본 cmdline을 백업 (jobs 출력용)
    char original_cmdline[MAXLINE];
    strncpy(original_cmdline, trim_cmdline, MAXLINE - 1);
    original_cmdline[MAXLINE - 1] = '\0';
    if (bg) {
        strcat(original_cmdline, " &");
    }

    if (bg) {
        // 백그라운드 파이프라인 실행
        add_job(pgid, original_cmdline, RUNNING);
        printf("[%d] %d\n", next_job_id - 1, pgid);
    } else {
        // 포그라운드 실행
        tcsetpgrp(STDIN_FILENO, pgid);
        add_job(pgid, original_cmdline, RUNNING);

        int status;
        // 프로세스 그룹 전체 대기
        if (waitpid(-pgid, &status, WUNTRACED) < 0) {
            if (errno != EINTR) {
                perror("waitpid error");
            }
        }
        
        // 쉘로 제어권 돌려놓기
        tcsetpgrp(STDIN_FILENO, getpgrp());
        
        // Ctrl+Z로 중지된 경우 job에서 삭제하지 않음
        if (WIFSTOPPED(status)) {
            // 여기서 상태를 명시적으로 STOPPED로 설정
            job_t *job = find_job_by_pgid(pgid);
            if (job) {
                job->state = STOPPED;
            }
        } else {
            // 정상 종료 또는 신호로 종료된 경우
            delete_job(pgid);
        }
    }

    // 메모리 정리
    for (int i = 0; i < n_cmds; ++i) {
        if (cmds[i]) free(cmds[i]);
    }
    pipeline_count = 0;
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

int add_job(pid_t pgid, const char *cmdline, job_state state) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].job_id == 0) {
            jobs[i].job_id = next_job_id++;
            jobs[i].pgid = pgid;
            jobs[i].state = state;
            strncpy(jobs[i].cmdline, cmdline, MAXLINE - 1);
            jobs[i].cmdline[MAXLINE - 1] = '\0';

            previous_job = current_job;
            current_job = jobs[i].job_id;
            return jobs[i].job_id;
        }
    }
    fprintf(stderr, "myshell: too many jobs\n");
    return -1;
}

void delete_job(pid_t pgid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == pgid) {
            int job_id = jobs[i].job_id;
            
            // current_job, previous_job 업데이트
            if (current_job == job_id) {
                current_job = previous_job;
                previous_job = 0;
                
                // 다른 작업 중 가장 최근 작업을 previous_job으로 설정
                for (int j = 0; j < MAXJOBS; j++) {
                    if (jobs[j].job_id != 0 && jobs[j].job_id != current_job) {
                        previous_job = jobs[j].job_id;
                        break;
                    }
                }
            } else if (previous_job == job_id) {
                previous_job = 0;
                
                // 다른 작업 중 가장 최근 작업을 previous_job으로 설정
                for (int j = 0; j < MAXJOBS; j++) {
                    if (jobs[j].job_id != 0 && jobs[j].job_id != current_job) {
                        previous_job = jobs[j].job_id;
                        break;
                    }
                }
            }
            
            jobs[i].job_id = 0;
            jobs[i].pgid = 0;
            jobs[i].cmdline[0] = '\0';
            jobs[i].state = RUNNING;
            return;
        }
    }
}

job_t *find_job_by_id(int job_id) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].job_id == job_id) {
            return &jobs[i];
        }
    }
    return NULL;
}

job_t *find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == pgid) {
            return &jobs[i];
        }
    }
    return NULL;
}

void list_jobs() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].job_id != 0) {
            char indicator = ' ';
            if (jobs[i].job_id == current_job) indicator = '+';
            else if (jobs[i].job_id == previous_job) indicator = '-';

            printf("[%d]%c  %s                 %s", 
                  jobs[i].job_id, 
                  indicator,
                  jobs[i].state == RUNNING ? "Running" : "Stopped",
                  jobs[i].cmdline);
            
            if (jobs[i].cmdline[strlen(jobs[i].cmdline)-1] != '\n') 
                printf("\n");
        }
    }
}

void do_fg(int job_id) {
    job_t *job = find_job_by_id(job_id);
    if (!job) {
        fprintf(stderr, "myshell: fg: %d: no such job\n", job_id);
        return;
    }
    
    // 명령어 출력 시 &를 제거
    char clean_cmdline[MAXLINE];
    strncpy(clean_cmdline, job->cmdline, MAXLINE - 1);
    clean_cmdline[MAXLINE - 1] = '\0';

    // &와 그 앞 공백 제거
    char *amp = strrchr(clean_cmdline, '&');
    if (amp) {
        while (amp > clean_cmdline && isspace(*(amp - 1))) amp--;
        *amp = '\0';  // 문자열 자르기
    }

    printf("%s\n", clean_cmdline);

    tcsetpgrp(STDIN_FILENO, job->pgid);
    job->state = RUNNING;
    Kill(-job->pgid, SIGCONT);

    int status;
    Waitpid(-job->pgid, &status, WUNTRACED); // 포그라운드 실행
    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (WIFSTOPPED(status)) {
        job->state = STOPPED;
        printf("\n[%d]+  Stopped                 %s", job->job_id, job->cmdline);
        if (job->cmdline[strlen(job->cmdline) - 1] != '\n') {
            printf("\n");
        }
    } else {
        delete_job(job->pgid);
    }
}

void do_bg(int job_id) {
    job_t *job = find_job_by_id(job_id);
    if (!job) {
        fprintf(stderr, "myshell: bg: %d: no such job\n", job_id);
        return;
    }

    job->state = RUNNING;
    Kill(-job->pgid, SIGCONT);

    // 명령어 출력 형식 조정
    char clean_cmdline[MAXLINE];
    strncpy(clean_cmdline, job->cmdline, MAXLINE - 1);
    clean_cmdline[MAXLINE - 1] = '\0';
    
    // & 제거
    char *amp = strrchr(clean_cmdline, '&');
    if (amp) {
        *amp = '\0';
        while (amp > clean_cmdline && isspace(*(amp-1))) {
            amp--;
            *amp = '\0';
        }
    }
    
    // 출력 포맷 조정 - 한 줄에 모든 내용 출력
    char indicator = ' ';
    if (job->job_id == current_job) indicator = '+';
    else if (job->job_id == previous_job) indicator = '-';
    
    printf("[%d]%c %s &\n", 
           job->job_id, 
           indicator,
           clean_cmdline);
}

void do_kill(int job_id) {
    job_t *job = find_job_by_id(job_id);
    if (!job) {
        fprintf(stderr, "myshell: kill: %%%d: no such job\n", job_id);
        return;
    }

    // 작업을 완전히 종료시키기 위해 SIGTERM 사용
    Kill(-job->pgid, SIGTERM);
}