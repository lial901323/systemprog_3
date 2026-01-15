
#include <stdio.h>    
#include <stdlib.h>     
#include <string.h>   
#include <unistd.h>     
#include <sys/wait.h>   
#include <errno.h>    

#define PROMPT   "bash-mini$ "
#define MAX_LINE 1024
#define MAX_ARGS 128


static int read_line(char *buf, size_t size) {
    if (fgets(buf, (int)size, stdin) == NULL) {
        return 0; // EOF or read error
    }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }
    return 1;
}


static int parse_line(char *line, char *argv[], int max_args) {
    int argc = 0;

    // delimiters are space and tab as required
    char *token = strtok(line, " \t");
    while (token != NULL && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}


static char *find_command_path(const char *cmd) {
    char path[MAX_LINE];

    // Step 1: Search in HOME directory
    const char *home = getenv("HOME");
    if (home != NULL) {
        snprintf(path, sizeof(path), "%s/%s", home, cmd);
        // access(..., X_OK) checks that file exists AND is executable
        if (access(path, X_OK) == 0) {
            return strdup(path);
        }
    }

    // Step 2: Search in /bin
    snprintf(path, sizeof(path), "/bin/%s", cmd);
    if (access(path, X_OK) == 0) {
        return strdup(path);
    }

    // Not found
    return NULL;
}

static void execute_command(char *argv[], int argc) {
    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "exit") == 0) {
        // Exit shell loop and end program
        exit(0);
    }

   
    if (strcmp(argv[0], "cd") == 0) {
        if (argc < 2) {
            fprintf(stderr, "cd: missing argument\n");
            return;
        }

        // chdir is the system call for changing working directory
        if (chdir(argv[1]) != 0) {
            perror("cd"); 
        }
        return;
    }

   
    char *full_path = find_command_path(argv[0]);
    if (full_path == NULL) {
        // Required error format:
        printf("[%s]: Unknown Command\n", argv[0]);
        return;
    }

    pid_t pid = fork(); // create child process
    if (pid < 0) {
        perror("fork");
        free(full_path);
        return;
    }

    if (pid == 0) {
        execv(full_path, argv);

        perror("execv");
        _exit(127); // common shell convention for exec failure
    }

    // Parent process: wait for child to finish
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        free(full_path);
        return;
    }

    // Report return code
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        printf("Command finished successfully. Return code: %d\n", code);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Command terminated by signal: %d\n", sig);
    } else {
        printf("Command ended (unknown status)\n");
    }

    free(full_path);
}

int main(void) {
    char line[MAX_LINE];
    char *argv[MAX_ARGS];

    // Main shell loop (infinite until exit or EOF)
    while (1) {
        // 1) Prompt
        printf("%s", PROMPT);
        fflush(stdout);

        // 2) Read
        if (!read_line(line, sizeof(line))) {
            // EOF (Ctrl+D) or error: exit shell gracefully
            printf("\n");
            break;
        }

        // 3) Parse (NOTE: parse_line modifies 'line' in-place)
        int argc = parse_line(line, argv, MAX_ARGS);

        // 4) Execute
        execute_command(argv, argc);
    }

    return 0;
}
