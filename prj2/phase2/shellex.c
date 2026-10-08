/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#include <string.h>

#define MAXARGS   128
#define MAXLINE   1024
#define MAX_PIPE_CMDS  10

/* Command list management structure */
typedef struct cmd_node {
    char* data;              /* Command string */
    struct cmd_node* next;   /* Next node */
} cmd_node;

typedef struct cmd_list {
    cmd_node* head;          /* List start */
    cmd_node* tail;          /* List end */
    int size;                /* Number of commands */
} cmd_list;

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

int main() 
{
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
        /* Read */
        printf("CSE4100-SP-P2> ");                   
        fgets(cmdline, MAXLINE, stdin); 
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    } 
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    
    /* Check if command contains pipe character */
    if (strchr(buf, '|')) {
        cmd_list pipe_cmds;
        init_list(&pipe_cmds);
        
        /* Split command by pipes */
        split_by_pipe(cmdline, &pipe_cmds);
        
        /* Check for background execution */
        int bg = 0;
        char* last_cmd = get_cmd(&pipe_cmds, pipe_cmds.size - 1);
        if (last_cmd) {
            char* amp = strrchr(last_cmd, '&');
            if (amp) {
                bg = 1;
                *amp = ' '; /* Remove & */
            }
        }
        
        /* Execute piped commands */
        run_piped_cmds(&pipe_cmds, bg);
        
        /* Clean up resources */
        clear_list(&pipe_cmds);
        return;
    }
    
    /* Handle regular command */
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */

    if (!builtin_command(argv)) { /* Not a builtin command */
        if ((pid = fork()) == 0) { /* Child runs user job */
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        /* Parent waits for foreground job to terminate */
        if (!bg) {
            int status;
            if (waitpid(pid, &status, 0) < 0)
                unix_error("waitfg: waitpid error");
        }
        else
            printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit")) /* quit command */
        exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
        return 1;
    if (!strcmp(argv[0], "cd")) { /* cd command */
        if (argv[1] == NULL)
            fprintf(stderr, "cd: missing argument\n");
        else if (chdir(argv[1]) < 0)
            perror("cd error");
        return 1;
    }
    return 0;     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    int argc = 0;            /* Number of args */
    char *p = buf;           /* Pointer to current character */
    char *start;             /* Pointer to start of current arg */
    int quote = 0;           /* Inside quotes flag */
    int bg = 0;              /* Background job flag */
    
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    
    /* Skip leading spaces */
    while (*p && (*p == ' '))
        p++;
    
    start = p;
    
    /* Parse the arguments */
    while (*p) {
        if (*p == '"') {
            /* Toggle quote flag */
            quote = !quote;
            
            if (quote) {
                /* Start of a quoted string, move start to next char */
                start = p + 1;
            } else {
                /* End of quoted string */
                *p = '\0';  /* Replace closing quote with null */
                argv[argc++] = start;
                start = p + 1;
            }
        } else if (*p == ' ' && !quote) {
            /* End of an argument */
            *p = '\0';
            
            /* Skip empty arguments */
            if (p > start) {
                argv[argc++] = start;
            }
            
            /* Find start of next argument */
            p++;
            while (*p && (*p == ' '))
                p++;
            
            start = p;
            continue;
        }
        
        p++;
    }
    
    /* Null terminate the argument list */
    argv[argc] = NULL;
    
    /* Check for background job */
    if (argc > 0 && !strcmp(argv[argc-1], "&")) {
        bg = 1;
        argv[--argc] = NULL;
    }
    
    return bg;
}
/* $end parseline */

/* Initialize command list function */
void init_list(cmd_list* list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/* Add command to list function */
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

/* Get command at specific index from list */
char* get_cmd(cmd_list* list, int index) {
    if (index < 0 || index >= list->size)
        return NULL;
    
    cmd_node* current = list->head;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    
    return current->data;
}

/* Clear command list function */
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

/* Split string by pipe and add to list - with quote handling */
int split_by_pipe(char* input, cmd_list* list) {
    char input_copy[MAXLINE];
    strcpy(input_copy, input);
    
    int i = 0;
    int start = 0;
    int in_quotes = 0;
    
    /* Split by pipe, but ignore pipes inside quotes */
    while (input_copy[i] != '\0') {
        if (input_copy[i] == '"') {
            in_quotes = !in_quotes;
        } else if (input_copy[i] == '|' && !in_quotes) {
            input_copy[i] = '\0';
            
            char* cmd = &input_copy[start];
            
            /* Remove leading whitespace */
            while (*cmd == ' ')
                cmd++;
                
            /* Add non-empty command to list */
            if (*cmd != '\0') {
                add_cmd(list, cmd);
            }
            
            start = i + 1;
        }
        i++;
    }
    
    /* Process last command */
    if (input_copy[start] != '\0') {
        char* cmd = &input_copy[start];
        
        /* Remove leading whitespace */
        while (*cmd == ' ')
            cmd++;
            
        if (*cmd != '\0') {
            add_cmd(list, cmd);
        }
    }
    
    return list->size;
}

/* Execute piped commands function */
void run_piped_cmds(cmd_list* cmd_list, int bg) {
    int cmd_count = cmd_list->size;
    int pipe_fds[MAX_PIPE_CMDS][2];
    pid_t pids[MAX_PIPE_CMDS];
    int i, status;
    
    /* Create pipes for each command */
    for (i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipe_fds[i]) < 0) {
            unix_error("pipe error");
            return;
        }
    }
    
    /* Execute each command */
    for (i = 0; i < cmd_count; i++) {
        char* cmd = get_cmd(cmd_list, i);
        char cmd_buf[MAXLINE];
        strcpy(cmd_buf, cmd);
        
        char* argv[MAXARGS];
        parseline(cmd_buf, argv);
        
        /* Check for empty command */
        if (argv[0] == NULL)
            continue;
        
        /* Check for builtin command */
        if (builtin_command(argv))
            continue;
        
        /* Create child process */
        if ((pids[i] = fork()) == 0) {
            /* Set up pipe input (except for first command) */
            if (i > 0) {
                dup2(pipe_fds[i-1][0], STDIN_FILENO);
            }
            
            /* Set up pipe output (except for last command) */
            if (i < cmd_count - 1) {
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }
            
            /* Close all unused pipe file descriptors */
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            /* Execute command */
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
    }
    
    /* Close all pipe file descriptors in parent */
    for (i = 0; i < cmd_count - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
    
    /* Wait for all child processes if foreground execution */
    if (!bg) {
        for (i = 0; i < cmd_count; i++) {
            waitpid(pids[i], &status, 0);
        }
    }
    else {
        /* Print background process info */
        printf("[1] %d\n", pids[cmd_count-1]);
    }
}