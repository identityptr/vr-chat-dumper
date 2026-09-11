#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void exec_child(const char *directory, const char *const arguments[])
{
    if (directory != NULL && chdir(directory) != 0) {
        perror(directory);
        _exit(126);
    }

    execvp(arguments[0], (char *const *)arguments);
    perror(arguments[0]);
    _exit(127);
}

static int wait_child(pid_t child)
{
    int status;

    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return -1;
        }
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return -1;
}

int process_run(const char *directory, const char *const arguments[])
{
    pid_t child = fork();

    if (child == -1) {
        perror("fork");
        return -1;
    }
    if (child == 0)
        exec_child(directory, arguments);

    return wait_child(child);
}
