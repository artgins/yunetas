/****************************************************************************
 *              RUN_EXECUTABLE.C
 *              Run a executable
 *
 *              Copyright (c) 2015 Niyamaka.
 *              Copyright (c) 2025-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <pty.h>
#include <spawn.h>
#include "run_command.h"

/***************************************************************************
 *      Prototypes
 ***************************************************************************/

/***************************************************************************
 *  Run a command and get the output
 ***************************************************************************/
PUBLIC gbuffer_t *run_command(const char *command) // use popen(), synchronous
{
    FILE *fp;
    char temp[4*1024];

    /* Open the command for reading. */
    snprintf(temp, sizeof(temp), "%s 2>&1", command);
    fp = popen(temp, "r");
    if(fp == NULL) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Can't exec command",
            "command",      "%s", command,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    if(!gbuf) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Can't create gbuffer",
            NULL
        );
        return NULL;
    }

    /* Read the output a line at a time - output it. */
    while(fgets(temp, sizeof(temp), fp) != NULL) {
        gbuffer_append(gbuf, temp, strlen(temp));
    }
    /* close */
    pclose(fp);

    return gbuf;
}

/***************************************************************************
 *  Run the program `path` with `argv` and wait for its end. No shell: argv
 *  reaches the program as it is. The program inherits the signal mask and
 *  the ignored signals of the caller, as across any exec.
 *
 *  Return the status of waitpid(2), 0 when the program exited with 0, or -1
 *  when it could not be run or waited for (logged).
 *
 *  Unlike system(3), SIGINT and SIGQUIT of the caller are not set to SIG_IGN
 *  while it waits: a yuno takes them from a signalfd, and ignoring them
 *  would throw away a stop that arrives in the meantime.
 ***************************************************************************/
PUBLIC int run_process2(const char *path, char *const argv[])
{
    if(empty_string(path) || !argv || !argv[0]) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "No program or no argv to run",
            "path",         "%s", path?path:"",
            NULL
        );
        return -1;
    }

    pid_t pid;
    int ret = posix_spawn(&pid, path, NULL, NULL, argv, environ);
    if(ret != 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "posix_spawn() FAILED",
            "path",         "%s", path,
            "errno",        "%d", ret,
            "strerror",     "%s", strerror(ret),
            NULL
        );
        return -1;
    }

    int status;
    while(waitpid(pid, &status, 0) < 0) {
        if(errno != EINTR) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "waitpid() FAILED",
                "path",         "%s", path,
                "pid",          "%d", (int)pid,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return -1;
        }
    }

    return status;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int pty_sync_spawn(
    const char *command
)
{
    int master, pid;

    struct winsize size = {24, 80, 0, 0 };
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    pid = forkpty(&master, NULL, NULL, &size);
    if (pid < 0) {
        // Can't fork
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "forkpty() FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;

    } else if (pid == 0) {
        // Child
        int ret = execlp("/bin/sh","/bin/sh", "-c", command, (char *)NULL);
        if (ret < 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "execlp() FAILED",
                "command",      "%s", command,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            _exit(-errno);
        }
    } else {
        // Parent
        while(1) {
            fd_set read_fd;
            fd_set write_fd;
            fd_set except_fd;

            FD_ZERO(&read_fd);
            FD_ZERO(&write_fd);
            FD_ZERO(&except_fd);

            FD_SET(master, &read_fd);
            FD_SET(STDIN_FILENO, &read_fd);

            select(master+1, &read_fd, &write_fd, &except_fd, NULL);

            char input;
            char output;

            if (FD_ISSET(master, &read_fd)) {
                if (read(master, &output, 1) != -1) {   // read from program
                    ssize_t x = write(STDOUT_FILENO, &output, 1);   // write to tty
                    if(x) {} // avoid warning
                } else {
                    break;
                }
            }

            if (FD_ISSET(STDIN_FILENO, &read_fd)) {
                ssize_t x = read(STDIN_FILENO, &input, 1);  // read from tty
                if(x) {} // avoid warning
                x = write(master, &input, 1);       // write to program
                if(x) {} // avoid warning
            }

            int status;
            if (waitpid(pid, &status, WNOHANG) && WIFEXITED(status)) {
                /*
                 *  Drain any data still buffered in the master pty
                 *  before returning.  Without this, short-running
                 *  non-interactive commands (cat, head, jq …) lose
                 *  their tail: the parent reads one byte per loop
                 *  iteration, so a 4 KiB-buffered output that arrives
                 *  in one shot gets a single byte drained — then the
                 *  WIFEXITED check fires on the next iteration and we
                 *  return, discarding the other 4095 bytes.
                 *
                 *  Reading in larger chunks here is safe because the
                 *  child has already exited; we just want to flush
                 *  whatever's still in the kernel's pty buffer.  On
                 *  Linux the master read returns -1 with errno=EIO
                 *  once the slave side is fully closed, which breaks
                 *  the drain loop.
                 */
                char drain_buf[4096];
                ssize_t n;
                while((n = read(master, drain_buf, sizeof(drain_buf))) > 0) {
                    ssize_t w = write(STDOUT_FILENO, drain_buf, n);
                    if(w) {} // avoid warning
                }
                return 0;
            }
        }
        return -1;
    }

    return -1;
}
