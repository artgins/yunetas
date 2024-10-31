/****************************************************************************
 *          launch_async_program.c
 *
 *          Launch a program without waiting his output
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include "launch_daemon.h"

/*
    Returning the First Child's PID: The first fork PID is returned directly.
    This allows the main program to keep a reference to the process that will become the daemon.

    Parent Process Doesn’t Wait: The parent doesn’t wait for the first child,
    so it continues executing immediately after forking.

    Returning pid: launch_program now returns the PID of the first child,
    which will remain the same after the second fork.

    Managing the Process: The main program can now use the returned PID to monitor
    or control the daemonized process, for example,
    by calling kill(child_pid, SIGTERM) to terminate it.
 */

/***************************************************************
 *              Constants
 ***************************************************************/
#define MAX_ARGS 256 // Maximum number of arguments

/***************************************************************************
 *  Function to launch a program as a daemonized process and return its PID
 ***************************************************************************/
pid_t launch_daemon(BOOL redirect_stdio_to_null, const char *program, ...)
{
#ifdef __linux__
    if (program == NULL) {
        print_error(0, "launch_program(): Program name is NULL.");
        return -1;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        print_error(0, "launch_program() pipe failed '%s'", program);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        print_error(0, "launch_program() fork failed '%s'", program);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    } else if (pid > 0) {
        // Parent process
        close(pipe_fd[1]);  // Close write end of the pipe
        int execvp_status;
        ssize_t bytes = read(pipe_fd[0], &execvp_status, sizeof(execvp_status));
        close(pipe_fd[0]);

        if (bytes > 0) {
            // If we received data, execvp failed
            print_error(0, "launch_program() Failed to launch program: execvp error %d '%s'", execvp_status, program);
            return -1;
        }

        return pid;  // Return PID of the first child
    }

    // First child process: create another child using double fork technique
    pid_t second_pid = fork();
    if (second_pid < 0) {
        print_error(0, "launch_program() second fork failed '%s'", program);
        exit(EXIT_FAILURE);
    } else if (second_pid > 0) {
        // First child exits immediately to avoid zombie
        exit(0);
    }

    // Second child becomes the daemonized process
    if (setsid() == -1) {
        print_error(0, "launch_program() setsid failed '%s'", program);
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);  // Ignore SIGHUP

    // Redirect standard file descriptors to /dev/null
    if(redirect_stdio_to_null) {
        int fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            print_error(0, "launch_program() Failed to open /dev/null '%s'", program);
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }

    // Close the read end of the pipe in the daemon
    close(pipe_fd[0]);

    // Prepare arguments for execvp
    va_list args;
    va_start(args, program);

    const char *arg;
    char *argv[MAX_ARGS];
    int i = 0;

    argv[i++] = (char *)program;
    while ((arg = va_arg(args, const char *)) != NULL && i < MAX_ARGS - 1) {
        argv[i++] = (char *)arg;
    }
    argv[i] = NULL;  // Null-terminate the argument list

    va_end(args);

    // Execute the program
    if (execvp(program, argv) == -1) {
        int err = errno;  // Capture the execvp error code
        print_error(0, "launch_program() execvp failed '%s', errno %d '%s'", program, err, strerror(err));
        write(pipe_fd[1], &err, sizeof(err));  // Send error to parent
        exit(EXIT_FAILURE);
    }

    close(pipe_fd[1]);  // Close write end of pipe if execvp succeeds

#endif /* __linux__ */
    return 0;  // This line is never reached
}


#ifdef TESTING

/***************************************************************************
 *  Function to handle signals, specifically cleaning up child processes
 ***************************************************************************/
#include <sys/wait.h>

// Function to handle signals, specifically cleaning up child processes
void handle_signals(int sig) {
    if (sig == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {}  // Reap zombie processes
    }
}

PRIVATE void handle_signals(int sig)
{
    if (sig == SIGCHLD) {
        // Reap any zombie child processes
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }
}

int main() {
    // Set up a signal handler to clean up child processes
    struct sigaction sa;
    sa.sa_handler = &handle_signals;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        print_error(0, "launch_program() sigaction failed");
        return EXIT_FAILURE;
    }

    printf("Launching program without waiting for response.\n");

    // Launch a program as a detached daemon process
    pid_t child_pid = launch_program("nonexistent_program_name", "arg1", "arg2", NULL);
    if (child_pid < 0) {
        print_error(0, "launch_program()Failed to launch program\n");
        return EXIT_FAILURE;
    }

    printf("Launched program with PID: %d\n", child_pid);

    // Continue with other work in the main program
    printf("Continuing with main program execution.\n");

    return EXIT_SUCCESS;
}
#endif
