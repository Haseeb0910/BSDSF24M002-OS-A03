#include "shell.h"
#include <fcntl.h>

/*
 * apply_redirections: called in the CHILD, after fork() and before execvp().
 * Finds "<" and ">" in arglist, redirects stdin/stdout to the named file,
 * and removes the operator and filename from arglist so that execvp()
 * only sees the real command and its arguments.
 */
static void apply_redirections(char** arglist) {
    for (int i = 0; arglist[i] != NULL; ) {
        int is_in  = (strcmp(arglist[i], "<") == 0);
        int is_out = (strcmp(arglist[i], ">") == 0);

        if (!is_in && !is_out) {
            i++;
            continue;
        }
        if (arglist[i + 1] == NULL) {
            fprintf(stderr, "syntax error: expected a file name after '%s'\n", arglist[i]);
            exit(1);
        }

        int fd;
        if (is_in)
            fd = open(arglist[i + 1], O_RDONLY);
        else
            fd = open(arglist[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(arglist[i + 1]);
            exit(1);
        }

        dup2(fd, is_in ? STDIN_FILENO : STDOUT_FILENO);
        close(fd);                       // the duplicate is enough

        // remove the two words (operator + file name): shift the rest left by 2,
        // stopping after the NULL terminator has been copied
        int j = i;
        do {
            arglist[j] = arglist[j + 2];
            j++;
        } while (arglist[j - 1] != NULL);
        // do not advance i: a new word now sits at position i
    }
}

// Returns the index of the first "|" in arglist, or -1 if there is none.
static int find_pipe(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++)
        if (strcmp(arglist[i], "|") == 0)
            return i;
    return -1;
}

/*
 * execute_pipe: runs "left | right". pos is the index of "|" in arglist.
 * The shell creates ONE pipe and forks TWO children:
 *   left child  (writer): its stdout  -> write end of the pipe
 *   right child (reader): its stdin   <- read end of the pipe
 */
static int execute_pipe(char** arglist, int pos) {
    char** right = arglist + pos + 1;

    if (pos == 0 || right[0] == NULL) {
        fprintf(stderr, "syntax error near '|'\n");
        return -1;
    }
    if (find_pipe(right) >= 0) {
        fprintf(stderr, "only one pipe per command is supported\n");
        return 1;
    }

    int fd[2];                       // fd[0] = read end, fd[1] = write end
    if (pipe(fd) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        perror("fork failed");
        close(fd[0]); close(fd[1]);
        return 1;
    }
    if (left_pid == 0) {             // ---- left child (writer) ----
        arglist[pos] = NULL;         // cut the line at '|' (child's copy only)
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);                // close BOTH original ends: the child
        close(fd[1]);                // must not keep the pipe open
        apply_redirections(arglist);
        execvp(arglist[0], arglist);
        perror("Command not found");
        exit(1);
    }

    pid_t right_pid = fork();
    if (right_pid < 0) {
        perror("fork failed");
        close(fd[0]); close(fd[1]);
        waitpid(left_pid, NULL, 0);
        return -1;
    }
    if (right_pid == 0) {            // ---- right child (reader) ----
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        apply_redirections(right);
        execvp(right[0], right);
        perror("Command not found");
        exit(1);
    }

    // ---- parent: close its copies of the pipe, then wait for both ----
    close(fd[0]);
    close(fd[1]);
    int right_status;
    waitpid(left_pid, NULL, 0);
    waitpid(right_pid, &right_status, 0);
    // like bash: the status of a pipeline is the status of its LAST command
    return WIFEXITED(right_status) ? WEXITSTATUS(right_status) : 1;
}

int execute(char* arglist[], int background) {
    int pos = find_pipe(arglist);
    if (pos >= 0) {
        if (background) {
            fprintf(stderr, "background pipelines are not supported\n");
            return 1;
        }
        return execute_pipe(arglist, pos);
    }

    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            apply_redirections(arglist);
            execvp(arglist[0], arglist);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            if (background) {            // do NOT wait: go straight back to the prompt
                add_job(cpid, arglist);
                printf("[started in background, pid %d]\n", cpid);
                return 0;
            }
            waitpid(cpid, &status, 0);
            // exit code of the child: 0 = success, non-zero = failure
            return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

/*
 * handle_builtin: runs commands that must execute inside the shell process
 * itself (a child process cannot change the parent's directory or exit it).
 * Returns 1 if arglist[0] was a built-in (already handled), 0 otherwise.
 */
int handle_builtin(char** arglist) {

    // NAME=value is an assignment, not a program: store it in the shell
    if (is_assignment(arglist[0])) {
        if (arglist[1] != NULL)
            fprintf(stderr, "%s: put the value in quotes if it contains spaces\n", arglist[0]);
        else
            var_assign(arglist[0]);
        return 1;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        const char* dir = arglist[1];
        if (dir == NULL)              // "cd" alone goes to HOME
            dir = getenv("HOME");
        if (dir == NULL || chdir(dir) != 0)
            fprintf(stderr, "cd: %s: %s\n", dir ? dir : "(no HOME)", strerror(errno));
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>   change the current directory\n");
        printf("  exit       exit the shell\n");
        printf("  help       show this help message\n");
        printf("  history    show previously entered commands\n");
        printf("  !n         re-run command number n from history\n");
        printf("  jobs       list running background jobs\n");
        printf("  set        show all shell variables\n");
        printf("  NAME=value assign a shell variable (read it with $NAME)\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        psh_hist_print();
        return 1;
    }
    
    if (strcmp(arglist[0], "set") == 0) {
        var_print_all();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    return 0;   // not a built-in
}

