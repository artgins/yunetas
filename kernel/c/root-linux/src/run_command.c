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
#include <fcntl.h>
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
 *  A program run from here starts with no signal blocked and every signal
 *  at its default, whatever the caller has. A yuno keeps its signals
 *  blocked for its signalfd, and a blocked or an ignored signal stays so
 *  across exec: up to 7.29.3 every program a yuno ran had SIGTERM, SIGINT,
 *  SIGQUIT, SIGALRM, SIGUSR1, SIGUSR2 and SIGPIPE blocked, so a `timeout`
 *  it ran could not stop its child with SIGTERM, nor a ctrl+c the command
 *  of a console.
 ***************************************************************************/
PRIVATE int spawnattr_with_clean_signals(posix_spawnattr_t *attr)
{
    sigset_t none;
    sigset_t all;
    sigemptyset(&none);
    sigfillset(&all);
    int ret = posix_spawnattr_init(attr);
    if(ret == 0) {
        ret = posix_spawnattr_setsigmask(attr, &none);
    }
    if(ret == 0) {
        ret = posix_spawnattr_setsigdefault(attr, &all);
    }
    if(ret == 0) {
        ret = posix_spawnattr_setflags(attr, POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF);
    }
    return ret;
}

/*
 *  The same in a forked child, before its exec. A signal that cannot be
 *  set (SIGKILL, SIGSTOP, those the C library keeps for itself) is left.
 */
PRIVATE void reset_signals_before_exec(void)
{
    for(int sig = 1; sig < NSIG; sig++) {
        struct sigaction sa;
        if(sigaction(sig, NULL, &sa) == 0 && sa.sa_handler == SIG_IGN) {
            sa.sa_handler = SIG_DFL;
            sigaction(sig, &sa, NULL);
        }
    }
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, NULL);
}

/***************************************************************************
 *  Run a command with /bin/sh -c and get its output, stdout and stderr in
 *  one, when it ends. Synchronous. NULL (logged) when it cannot be run.
 *  The command starts with clean signals (spawnattr_with_clean_signals()).
 *  Up to 7.29.3 it was popen() of the command with " 2>&1" appended, in a
 *  buffer of 4 KB that cut a longer command with nothing said, and the
 *  redirection took only the last command of a list.
 ***************************************************************************/
PUBLIC gbuffer_t *run_command(const char *command) // posix_spawn() of sh -c, synchronous
{
    if(empty_string(command)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "No command to run",
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    if(!gbuf) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Can't create gbuffer",
            "command",      "%s", command,
            NULL
        );
        return NULL;
    }

    int fds[2];
    if(pipe2(fds, O_CLOEXEC) < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "pipe2() FAILED",
            "command",      "%s", command,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gbuffer_decref(gbuf);
        return NULL;
    }

    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int ret = posix_spawn_file_actions_init(&actions);
    if(ret == 0) {
        ret = posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    }
    if(ret == 0) {
        ret = posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);
    }
    if(ret == 0) {
        ret = spawnattr_with_clean_signals(&attr);
    }
    pid_t pid = 0;
    if(ret == 0) {
        char *const argv[] = {"sh", "-c", (char *)command, NULL};
        ret = posix_spawn(&pid, "/bin/sh", &actions, &attr, argv, environ);
        posix_spawnattr_destroy(&attr);
    }
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if(ret != 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "posix_spawn() FAILED: command not run",
            "command",      "%s", command,
            "errno",        "%d", ret,
            "strerror",     "%s", strerror(ret),
            NULL
        );
        close(fds[0]);
        gbuffer_decref(gbuf);
        return NULL;
    }

    BOOL cut = FALSE;
    char bf[4*1024];
    while(1) {
        ssize_t n = read(fds[0], bf, sizeof(bf));
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "read() of the output FAILED: output cut",
                "command",      "%s", command,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            break;
        }
        if(n == 0) {
            break;
        }
        if(!cut && gbuffer_append(gbuf, bf, (size_t)n) != (size_t)n) {
            cut = TRUE;     // read on, so the command does not block on a full pipe
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "Output of the command too large: cut",
                "command",      "%s", command,
                "size",         "%lu", (unsigned long)gbuffer_leftbytes(gbuf),
                NULL
            );
        }
    }
    close(fds[0]);

    int status;
    while(waitpid(pid, &status, 0) < 0) {
        if(errno != EINTR) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "waitpid() FAILED",
                "command",      "%s", command,
                "pid",          "%d", (int)pid,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            break;
        }
    }

    return gbuf;
}

/***************************************************************************
 *  Run the program `path` with `argv` and wait for its end. No shell: argv
 *  reaches the program as it is. The program starts with clean signals
 *  (spawnattr_with_clean_signals()): no signal blocked, every one at its
 *  default. Up to 7.29.3 it inherited the caller's mask and ignores.
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
    posix_spawnattr_t attr;
    int ret = spawnattr_with_clean_signals(&attr);
    if(ret == 0) {
        ret = posix_spawn(&pid, path, NULL, &attr, argv, environ);
        posix_spawnattr_destroy(&attr);
    }
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
        reset_signals_before_exec();
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
