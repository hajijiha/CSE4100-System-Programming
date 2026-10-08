/* 
 * MyShell - Shell implementation with enhanced pipe functionality
 * CSE4100 System Programming
 */
#include "csapp.h"  
#include <errno.h>
#include <string.h>

#define MAXARGS   128
/* MAXLINE is already defined in csapp.h, so do not redefine it */
#define MAX_PIPE_CMDS  10
#define MAXJOBS   64

/* Job status definitions */
#define JOB_RUNNING     1
#define JOB_STOPPED     0
#define JOB_FOREGROUND  1
#define JOB_BACKGROUND  0

/* Command list management structure */
typedef struct cmd_node {
    char* data;              /* Command string */
    struct cmd_node* next;   /* Next node */
} cmd_node;

typedef struct cmd_list {
    cmd_node* head;          /* Start of the list */
    cmd_node* tail;          /* End of the list */
    int size;                /* Number of commands */
} cmd_list;

/* Job management structure */
typedef struct job {
    pid_t pid;               /* Process ID of the job */
    int jid;                 /* Job ID */
    int state;               /* JOB_RUNNING or JOB_STOPPED */
    int ground;              /* JOB_FOREGROUND or JOB_BACKGROUND */
    char cmdline[MAXLINE];   /* Command line */
} job_t;

/* Global variables */
job_t jobs[MAXJOBS];         /* Job array */
int nextjid = 1;             /* Next job ID */
volatile sig_atomic_t fg_pid = 0;  /* Current foreground process ID (signal-safe) */

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void init_list(cmd_list* list);
void add_cmd(cmd_list* list, char* cmd);
char* get_cmd(cmd_list* list, int index);
void clear_list(cmd_list* list);
int split_by_pipe(char* input, cmd_list* list);
void run_piped_cmds(cmd_list* cmd_list, int bg);

/* Job management functions */
void init_jobs(job_t *jobs);
void add_job(job_t *jobs, pid_t pid, int state, int ground, char *cmdline);
int delete_job(job_t *jobs, pid_t pid);
job_t *get_job_pid(job_t *jobs, pid_t pid);
job_t *get_job_jid(job_t *jobs, int jid);
int pid_to_jid(job_t *jobs, pid_t pid);
void list_jobs(job_t *jobs);
void wait_fg(pid_t pid);
void check_job_status(void);

/* Signal handlers */
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigchld_handler(int sig);

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    
    /* Set up signal handlers */
    Signal(SIGINT, sigint_handler);   /* Ctrl-C */
    Signal(SIGTSTP, sigtstp_handler);   /* Ctrl-Z */
    Signal(SIGCHLD, sigchld_handler);   /* Child process termination */
    
    /* Initialize job array */
    init_jobs(jobs);

    while (1) {
        /* Display prompt */
        check_job_status();
        printf("CSE4100-SP-P2> ");
        fflush(stdout); // Immediate output
        
        /* Read command line */
        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (errno == EINTR) {
                // Interrupted by signal
                errno = 0;
                clearerr(stdin);
                continue;
            } else if (feof(stdin)) {
                exit(0);
            }
        }
        
        /* Execute command */
        eval(cmdline);
    } 
}

/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list for execve() */
    char buf[MAXLINE];   /* Buffer to hold modified command line */
    int bg;              /* Should the job run in background or foreground? */
    pid_t pid;           /* Process ID */
    
    strcpy(buf, cmdline);
    
    /* Check if the command contains a pipe character */
    if (strchr(buf, '|')) {
        cmd_list pipe_cmds;
        init_list(&pipe_cmds);
        
        /* Split the command by pipes */
        split_by_pipe(cmdline, &pipe_cmds);
        
        /* Check for background execution */
        bg = 0;
        char* last_cmd = get_cmd(&pipe_cmds, pipe_cmds.size - 1);
        if (last_cmd) {
            int len = strlen(last_cmd);
            /* Check if the last command ends with '&' */
            for (int i = len - 1; i >= 0; i--) {
                if (last_cmd[i] == '&') {
                    bg = 1;
                    last_cmd[i] = ' '; /* Remove '&' */
                    break;
                } else if (last_cmd[i] != ' ' && last_cmd[i] != '\t' && last_cmd[i] != '\n') {
                    /* Stop if a non-whitespace character is found */
                    break;
                }
            }
        }
        
        /* Execute the piped commands */
        run_piped_cmds(&pipe_cmds, bg);
        
        /* Clean up resources */
        clear_list(&pipe_cmds);
        return;
    }
    
    /* Parse the command line */
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */
    
    /* Check for built-in commands */
    if (!strcmp(argv[0], "jobs")) {
        list_jobs(jobs);
        return;
    }
    
    if (!strcmp(argv[0], "fg")) {
        int jid;
        job_t *job;
        
        /* Check job ID */
        if (argv[1] == NULL) {
            printf("fg: argument required\n");
            return;
        }
        
        /* Parse job ID starting with '%' */
        if (argv[1][0] == '%') {
            jid = atoi(&argv[1][1]);
        } else {
            jid = atoi(argv[1]);
        }
        
        job = get_job_jid(jobs, jid);
        if (job == NULL) {
            printf("fg: %%%d: No such job\n", jid);
            return;
        }
        
        /* Change job status */
        job->state = JOB_RUNNING;
        job->ground = JOB_FOREGROUND;
        
        /* Set as current foreground job */
        fg_pid = job->pid;
        
        /* Print job info */
        printf("%s", job->cmdline);
        
        /* Send SIGCONT signal */
        kill(-job->pid, SIGCONT);
        
        /* Wait for the foreground job to complete */
        wait_fg(job->pid);
        
        return;
    }
    
    if (!strcmp(argv[0], "bg")) {
        int jid;
        job_t *job;
        
        /* Check job ID */
        if (argv[1] == NULL) {
            printf("bg: argument required\n");
            return;
        }
        
        /* Parse job ID starting with '%' */
        if (argv[1][0] == '%') {
            jid = atoi(&argv[1][1]);
        } else {
            jid = atoi(argv[1]);
        }
        
        job = get_job_jid(jobs, jid);
        if (job == NULL) {
            printf("bg: %%%d: No such job\n", jid);
            return;
        }
        
        /* Change job status */
        job->state = JOB_RUNNING;
        job->ground = JOB_BACKGROUND;
        
        /* Send SIGCONT signal */
        kill(-job->pid, SIGCONT);
        
        /* Print job info */
        printf("[%d] %s", job->jid, job->cmdline);
        
        return;
    }
    
    if (!strcmp(argv[0], "kill")) {
        int jid;
        job_t *job;
        
        /* Check job ID */
        if (argv[1] == NULL) {
            printf("kill: argument required\n");
            return;
        }
        
        /* Parse job ID starting with '%' */
        if (argv[1][0] == '%') {
            jid = atoi(&argv[1][1]);
        } else {
            jid = atoi(argv[1]);
        }
        
        job = get_job_jid(jobs, jid);
        if (job == NULL) {
            printf("kill: %%%d: No such job\n", jid);
            return;
        }
        
        /* First wake up a stopped process with SIGCONT */
        kill(-job->pid, SIGCONT);
        
        /* Then terminate with SIGKILL */
        kill(-job->pid, SIGKILL);
        
        return;
    }
    
    /* Handle a regular command */
    if (!builtin_command(argv)) { /* Not a built-in command */
        /* Create a child process */
        if ((pid = fork()) == 0) { 
            /* Create a new process group */
            setpgid(0, 0);
            
            /* Execute the command */
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(1);
            }
        }

        /* Parent process */
        /* Add the job */
        add_job(jobs, pid, JOB_RUNNING, bg ? JOB_BACKGROUND : JOB_FOREGROUND, cmdline);
        
        /* Wait for the foreground job */
        if (!bg) {
            fg_pid = pid;
            wait_fg(pid);
        }
        else {
            /* Print background job info */
            printf("[%d] %d %s", pid_to_jid(jobs, pid), pid, cmdline);
        }
    }
    return;
}

/* If the first argument is a built-in command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit")) /* quit command */
        exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore a solitary '&' */
        return 1;
    if (!strcmp(argv[0], "cd")) { /* cd command */
        if (argv[1] == NULL)
            fprintf(stderr, "cd: missing argument\n");
        else if (chdir(argv[1]) < 0)
            perror("cd error");
        return 1;
    }
    if (!strcmp(argv[0], "jobs") || !strcmp(argv[0], "fg") || 
        !strcmp(argv[0], "bg") || !strcmp(argv[0], "kill")) {
        return 1; /* Job control commands */
    }
    return 0;     /* Not a built-in command */
}

/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    int argc = 0;            /* Number of arguments */
    char *p = buf;           /* Pointer to current character */
    char *delim;             /* Points to the first space delimiter */
    int bg = 0;              /* Background job flag */
    
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with a space */
    
    /* Skip leading spaces */
    while (*p && (*p == ' '))
        p++;
    
    /* Build the argv list */
    while (*p) {
        /* Handle quoted strings as a single argument */
        if (*p == '"' || *p == '\'') {
            char quote = *p; /* Save the quote character */
            p++;             /* Skip the opening quote */
            
            /* Set delim to the matching closing quote */
            delim = strchr(p, quote);
            
            if (delim == NULL) {
                /* No matching quote found, treat the rest as a single argument */
                delim = buf + strlen(buf) - 1;
            }
            
            /* Null terminate this argument */
            *delim = '\0';
            
            /* Add the argument to the list */
            argv[argc++] = p;
            
            /* Move pointer p to the character after the closing quote */
            p = delim + 1;
        } else {
            /* Regular argument (no quotes) */
            /* Set delim to point to the next space */
            delim = strchr(p, ' ');
            
            /* Add the argument to the list */
            argv[argc++] = p;
            
            /* Move pointer p to the character after the delimiter */
            p = delim + 1;
        }
        
        /* Null terminate the current argument */
        *delim = '\0';
        
        /* Skip any additional spaces */
        while (*p && (*p == ' '))
            p++;
    }
    
    /* Null terminate the argument list */
    argv[argc] = NULL;
    
    /* Check for background job */
    if (argc > 0 && !strcmp(argv[argc-1], "&")) {
        bg = 1;
        argv[--argc] = NULL;
    }
    
    /* Also handle commands with '&' attached to the last argument */
    if (argc > 0) {
        char *last_arg = argv[argc-1];
        int len = strlen(last_arg);
        
        if (len > 0 && last_arg[len-1] == '&') {
            bg = 1;
            last_arg[len-1] = '\0';
            
            /* Remove the argument if it becomes empty */
            if (strlen(last_arg) == 0) {
                argv[--argc] = NULL;
            }
        }
    }
    
    return bg;
}

/* Initialize the command list */
void init_list(cmd_list* list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/* Add a command to the list */
void add_cmd(cmd_list* list, char* cmd) {
    cmd_node* new_node = (cmd_node*)malloc(sizeof(cmd_node));
    new_node->data = strdup(cmd);
    new_node->next = NULL;
    
    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->tail->next = new_node;
        list->tail = new_node;
    }
    
    list->size++;
}

/* Get the command at a specific index from the list */
char* get_cmd(cmd_list* list, int index) {
    if (index < 0 || index >= list->size)
        return NULL;
    
    cmd_node* current = list->head;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    
    return current->data;
}

/* Clear the command list */
void clear_list(cmd_list* list) {
    cmd_node* current = list->head;
    cmd_node* next;
    
    while (current != NULL) {
        next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/* Improved function to split piped commands */
int split_by_pipe(char* input, cmd_list* list) {
    char input_copy[MAXLINE];
    strcpy(input_copy, input);
    
    char* token;
    char* rest = input_copy;
    int in_quotes = 0;
    char quote_char = 0;
    int pos = 0;
    int start = 0;
    
    /* More robust parsing logic */
    while (input_copy[pos] != '\0') {
        /* Handle quotes */
        if ((input_copy[pos] == '"' || input_copy[pos] == '\'') && 
            (pos == 0 || input_copy[pos-1] != '\\')) {
            if (!in_quotes) {
                /* Start of a quoted segment */
                in_quotes = 1;
                quote_char = input_copy[pos];
            } else if (input_copy[pos] == quote_char) {
                /* Closing with the matching quote */
                in_quotes = 0;
                quote_char = 0;
            }
        }
        
        /* Pipe character detected (only when outside quotes) */
        if (input_copy[pos] == '|' && !in_quotes) {
            /* Replace the pipe character with a null terminator */
            input_copy[pos] = '\0';
            
            /* Extract and trim the command */
            char* cmd = &input_copy[start];
            while (*cmd == ' ' || *cmd == '\t') cmd++; /* Remove leading whitespace */
            
            /* Remove trailing whitespace */
            char* end = cmd + strlen(cmd) - 1;
            while (end > cmd && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *end-- = '\0';
            }
            
            /* If the command is not empty, add it to the list */
            if (*cmd != '\0') {
                add_cmd(list, cmd);
            }
            
            /* Update the start position for the next command */
            start = pos + 1;
        }
        
        pos++;
    }
    
    /* Process the last command (if it exists) */
    if (input_copy[start] != '\0') {
        char* cmd = &input_copy[start];
        while (*cmd == ' ' || *cmd == '\t') cmd++; /* Remove leading whitespace */
        
        /* Remove trailing whitespace */
        char* end = cmd + strlen(cmd) - 1;
        while (end > cmd && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end-- = '\0';
        }
        
        if (*cmd != '\0') {
            add_cmd(list, cmd);
        }
    }
    
    return list->size;
}

/* Improved function to execute piped commands */
void run_piped_cmds(cmd_list* cmd_list, int bg) {
    int cmd_count = cmd_list->size;
    int pipe_fds[MAX_PIPE_CMDS][2];
    pid_t pids[MAX_PIPE_CMDS];
    int i, status;
    
    /* Variable to store the process group ID of the first command */
    pid_t pgid = 0;
    
    /* Create a pipe for each command (except the last one) */
    for (i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipe_fds[i]) < 0) {
            perror("pipe error");
            return;
        }
    }
    
    /* Construct the complete command string (for job management) */
    char cmd_str[MAXLINE] = "";
    for (i = 0; i < cmd_count; i++) {
        if (i > 0) strcat(cmd_str, " | ");
        strcat(cmd_str, get_cmd(cmd_list, i));
    }
    if (bg) strcat(cmd_str, " &");
    
    /* Execute each command in the pipeline */
    for (i = 0; i < cmd_count; i++) {
        char* cmd = get_cmd(cmd_list, i);
        char cmd_buf[MAXLINE];
        strcpy(cmd_buf, cmd);
        
        /* Enhanced command parsing */
        char* argv[MAXARGS] = {NULL};
        int argc = 0;
        char* p = cmd_buf;
        char* token;
        int in_quotes = 0;
        char quote_char = 0;
        char* arg_start = p;
        
        /* Tokenize based on whitespace */
        while (*p) {
            if ((*p == '"' || *p == '\'') && (p == cmd_buf || *(p-1) != '\\')) {
                /* Handle quoted strings */
                if (!in_quotes) {
                    in_quotes = 1;
                    quote_char = *p;
                    arg_start = p + 1; /* Start argument immediately after the quote */
                } else if (*p == quote_char) {
                    /* Encounter closing quote */
                    *p = '\0'; /* Replace the closing quote with a null terminator */
                    argv[argc++] = arg_start;
                    in_quotes = 0;
                    arg_start = p + 1;
                }
            } else if ((*p == ' ' || *p == '\t') && !in_quotes) {
                /* Whitespace outside of quotes */
                *p = '\0';
                if (p > arg_start) { /* Skip empty arguments */
                    argv[argc++] = arg_start;
                }
                arg_start = p + 1;
            }
            p++;
        }
        
        /* Process the last argument */
        if (*arg_start && arg_start < p) {
            argv[argc++] = arg_start;
        }
        
        /* NULL terminate the argv array */
        argv[argc] = NULL;
        
        /* Skip empty commands */
        if (argv[0] == NULL)
            continue;
        
        /* Check for built-in commands */
        if (builtin_command(argv))
            continue;
        
        /* Fork a child process */
        if ((pids[i] = fork()) == 0) {
            /* For the first command, create a new process group */
            if (i == 0) {
                setpgid(0, 0);
            } else {
                /* For other commands, use the same process group as the first command */
                setpgid(0, pgid);
            }
            
            /* Set up pipe input (for commands after the first) */
            if (i > 0) {
                if (dup2(pipe_fds[i-1][0], STDIN_FILENO) < 0) {
                    perror("dup2 input error");
                    exit(1);
                }
            }
            
            /* Set up pipe output (for commands before the last) */
            if (i < cmd_count - 1) {
                if (dup2(pipe_fds[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2 output error");
                    exit(1);
                }
            }
            
            /* Close all pipe file descriptors */
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            /* Execute the command */
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(1);
            }
        } else if (pids[i] < 0) {
            /* Handle process creation error */
            perror("fork error");
            return;
        }
        
        /* Store PGID for the first command */
        if (i == 0) {
            pgid = pids[0];
            
            /* Ensure the process group is set (also in the parent process) */
            setpgid(pids[0], pids[0]);
        } else {
            /* Set other processes to the same process group */
            setpgid(pids[i], pgid);
        }
    }
    
    /* In the parent process, close all pipe file descriptors */
    for (i = 0; i < cmd_count - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
    
    /* Add the entire pipeline as a single job */
    add_job(jobs, pgid, JOB_RUNNING, 
            bg ? JOB_BACKGROUND : JOB_FOREGROUND, cmd_str);
    
    /* If running in the foreground */
    if (!bg) {
        fg_pid = pgid;
        
        /* Wait until the foreground job completes */
        wait_fg(pgid);
    }
    else {
        /* Print background job information */
        printf("[%d] %d %s\n", pid_to_jid(jobs, pgid), 
               pgid, cmd_str);
    }
}

/* Initialize the job array */
void init_jobs(job_t *jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        jobs[i].pid = 0;
        jobs[i].jid = 0;
        jobs[i].state = 0;
        jobs[i].ground = 0;
        jobs[i].cmdline[0] = '\0';
    }
}

/* Check for job status changes */
void check_job_status() {
    static int last_job_states[MAXJOBS] = {0};
    
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid > 0) {
            if (jobs[i].state != last_job_states[i]) {
                if (jobs[i].state == JOB_STOPPED) {
                    printf("[%d] Stopped %s\n", jobs[i].jid, jobs[i].cmdline);
                }
                last_job_states[i] = jobs[i].state;
            }
        } else {
            last_job_states[i] = 0;
        }
    }
}

/* Add a job to the job list */
void add_job(job_t *jobs, pid_t pid, int state, int ground, char *cmdline) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].jid = nextjid++;
            jobs[i].state = state;
            jobs[i].ground = ground;
            strcpy(jobs[i].cmdline, cmdline);
            if (nextjid > MAXJOBS)
                nextjid = 1;
            return;
        }
    }
    printf("add_job: Too many jobs\n");
}

/* Delete a job from the job list */
int delete_job(job_t *jobs, pid_t pid) {
    int i;
    
    if (pid < 1)
        return 0;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].pid = 0;
            jobs[i].jid = 0;
            jobs[i].state = 0;
            jobs[i].ground = 0;
            jobs[i].cmdline[0] = '\0';
            return 1;
        }
    }
    return 0;
}

/* Find a job by PID */
job_t *get_job_pid(job_t *jobs, pid_t pid) {
    int i;
    
    if (pid < 1)
        return NULL;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

/* Find a job by JID */
job_t *get_job_jid(job_t *jobs, int jid) {
    int i;
    
    if (jid < 1)
        return NULL;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].jid == jid) {
            return &jobs[i];
        }
    }
    return NULL;
}

/* Convert a PID to a JID */
int pid_to_jid(job_t *jobs, pid_t pid) {
    int i;
    
    if (pid < 1)
        return 0;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    }
    return 0;
}

/* List all jobs */
void list_jobs(job_t *jobs) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] ", jobs[i].jid);
            if (jobs[i].state == JOB_RUNNING) {
                printf("Running ");
            } else {
                printf("Stopped ");
            }
            printf("%s", jobs[i].cmdline);
            if (jobs[i].cmdline[strlen(jobs[i].cmdline)-1] != '\n') {
                printf("\n");
            }
        }
    }
}

/* Improved function to wait for a foreground job */
void wait_fg(pid_t pid) {
    sigset_t mask, prev;
    
    /* Wait until the state of the child process changes */
    
    /* Block the SIGCHLD signal */
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    
    /* Loop until fg_pid becomes 0 (set by the SIGCHLD handler) */
    while (fg_pid > 0) {
        sigsuspend(&prev);  /* Wait for SIGCHLD signal */
    }
    
    /* Restore the original signal mask */
    sigprocmask(SIG_SETMASK, &prev, NULL);
}

/* SIGINT (Ctrl-C) handler */
void sigint_handler(int sig) {
    /* Forward SIGINT to the foreground job */
    if (fg_pid > 0) {
        kill(-fg_pid, SIGINT);
    }
    return;
}

/* SIGTSTP (Ctrl-Z) handler */
void sigtstp_handler(int sig) {
    /* Forward SIGTSTP to the foreground job */
    if (fg_pid > 0) {
        job_t *job = get_job_pid(jobs, fg_pid);
        if (job != NULL) {
            job->state = JOB_STOPPED;
            job->ground = JOB_BACKGROUND;
            
            /* Print without newline to avoid prompt duplication */
            printf("\n[%d] Stopped %s", job->jid, job->cmdline);
            if (job->cmdline[strlen(job->cmdline)-1] != '\n') {
                printf("\n");
            }
        }
        
        /* Send the SIGTSTP signal to the job group */
        kill(-fg_pid, SIGTSTP);
        
        /* Reset the foreground job flag */
        fg_pid = 0;
    }
}

/* Improved SIGCHLD handler */
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    sigset_t mask, prev;
    
    /* Block SIGCHLD to prevent nested signal handling */
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    
    /* Process all terminated child processes */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        /* Update job status */
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            /* For normal termination or termination due to a signal */
            job_t *job = get_job_pid(jobs, pid);
            
            /* Reset fg_pid if the foreground job terminates */
            if (pid == fg_pid || (job && job->ground == JOB_FOREGROUND)) {
                fg_pid = 0;
            }
            
            /* Delete the job */
            if (job != NULL) {
                delete_job(jobs, pid);
            }
        } else if (WIFSTOPPED(status)) {
            /* Handle stopped processes */
            job_t *job = get_job_pid(jobs, pid);
            if (job != NULL) {
                job->state = JOB_STOPPED;
                if (job->ground == JOB_FOREGROUND) {
                    job->ground = JOB_BACKGROUND;
                    fg_pid = 0; /* The foreground job has been stopped */
                }
            }
        }
    }
    
    /* Restore the original signal mask */
    sigprocmask(SIG_SETMASK, &prev, NULL);
}
