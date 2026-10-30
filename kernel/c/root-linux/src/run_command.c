/****************************************************************************
 *              RUN_EXECUTABLE.C
 *              Run a executable
 *
 *              Copyright (c) 2015 Niyamaka.
 *              Copyright (c) 2025-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <signal.h>
#include <dirent.h>
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

#if !(defined(__GLIBC__) && __GLIBC_PREREQ(2, 34))
/***************************************************************************
 *  A C library with no posix_spawn_file_actions_addclosefrom_np() (glibc
 *  older than 2.34, that a source build may still have): each fd above
 *  stderr open now is closed one by one. 0 or the error.
 ***************************************************************************/
PRIVATE int add_close_of_open_fds(posix_spawn_file_actions_t *actions)
{
    DIR *d = opendir("/proc/self/fd");
    if(!d) {
        return errno;
    }
    int ret = 0;
    int dfd = dirfd(d);
    struct dirent *e;
    while(ret == 0 && (e = readdir(d)) != NULL) {
        char *end;
        long fd = strtol(e->d_name, &end, 10);
        if(end == e->d_name || *end || fd <= STDERR_FILENO || fd == dfd) {
            continue;
        }
        ret = posix_spawn_file_actions_addclose(actions, (int)fd);
    }
    closedir(d);
    return ret;
}
#endif

/***************************************************************************
 *  What a program run from here starts with, whatever the caller has: no
 *  signal blocked, every signal at its default, and no fd open but stdin,
 *  stdout and stderr (posix_spawn_file_actions_addclosefrom_np(), or one
 *  by one on a glibc older than 2.34). With `out_fd` >= 0 (run_command())
 *  its stdout and stderr go to `out_fd` and its stdin is /dev/null.
 *
 *  A yuno keeps its signals blocked for its signalfd, and a blocked or an
 *  ignored signal stays so across exec: up to 7.29.3 every program a yuno
 *  ran had SIGTERM, SIGINT, SIGQUIT, SIGALRM, SIGUSR1, SIGUSR2 and SIGPIPE
 *  blocked, so a `timeout` it ran could not stop its child with SIGTERM,
 *  nor a ctrl+c the command of a console. And it inherited every fd of the
 *  yuno without O_CLOEXEC, and held it open after the yuno closed it.
 *
 *  NULL when both are made (destroy both), else the call that failed, its
 *  error in *perr, and nothing to destroy.
 ***************************************************************************/
PRIVATE const char *spawn_setup(
    posix_spawn_file_actions_t *actions,
    posix_spawnattr_t *attr,
    int out_fd,
    int *perr
)
{
    const char *failed = NULL;
    int ret = posix_spawn_file_actions_init(actions);
    if(ret != 0) {
        *perr = ret;
        return "posix_spawn_file_actions_init()";
    }
    if(out_fd >= 0) {
        ret = posix_spawn_file_actions_addopen(actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        if(ret != 0) {
            failed = "posix_spawn_file_actions_addopen(/dev/null)";
        }
        if(!failed) {
            ret = posix_spawn_file_actions_adddup2(actions, out_fd, STDOUT_FILENO);
            if(ret != 0) {
                failed = "posix_spawn_file_actions_adddup2(stdout)";
            }
        }
        if(!failed) {
            ret = posix_spawn_file_actions_adddup2(actions, out_fd, STDERR_FILENO);
            if(ret != 0) {
                failed = "posix_spawn_file_actions_adddup2(stderr)";
            }
        }
    }
    if(!failed) {
#if defined(__GLIBC__) && __GLIBC_PREREQ(2, 34)
        ret = posix_spawn_file_actions_addclosefrom_np(actions, STDERR_FILENO + 1);
        if(ret != 0) {
            failed = "posix_spawn_file_actions_addclosefrom_np()";
        }
#else
        ret = add_close_of_open_fds(actions);
        if(ret != 0) {
            failed = "posix_spawn_file_actions_addclose() of /proc/self/fd";
        }
#endif
    }
    if(failed) {
        posix_spawn_file_actions_destroy(actions);
        *perr = ret;
        return failed;
    }

    ret = posix_spawnattr_init(attr);
    if(ret != 0) {
        posix_spawn_file_actions_destroy(actions);
        *perr = ret;
        return "posix_spawnattr_init()";
    }
    sigset_t none;
    sigset_t all;
    sigemptyset(&none);
    sigfillset(&all);
    ret = posix_spawnattr_setsigmask(attr, &none);
    if(ret != 0) {
        failed = "posix_spawnattr_setsigmask()";
    }
    if(!failed) {
        ret = posix_spawnattr_setsigdefault(attr, &all);
        if(ret != 0) {
            failed = "posix_spawnattr_setsigdefault()";
        }
    }
    if(!failed) {
        ret = posix_spawnattr_setflags(attr, POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF);
        if(ret != 0) {
            failed = "posix_spawnattr_setflags()";
        }
    }
    if(failed) {
        posix_spawnattr_destroy(attr);
        posix_spawn_file_actions_destroy(actions);
        *perr = ret;
        return failed;
    }
    return NULL;
}

/***************************************************************************
 *  Run a command with /bin/sh -c and get its output, stdout and stderr in
 *  one, when it ends. Synchronous. NULL (logged) when it cannot be run.
 *  The command starts as spawn_setup() says: clean signals, no fd of the
 *  caller, stdin on /dev/null (up to 7.29.3 it read the caller's).
 *  Up to 7.29.3 it was popen() of the command with " 2>&1" appended, in a
 *  buffer of 4 KB that cut a longer command with nothing said, and the
 *  redirection took only the last command of a list.
 *
 *  Its end is the end of its output: every process that still has the
 *  command's stdout or stderr holds it, a process left in the background
 *  too ("sleep 9 >/dev/null &" holds it 9 s: its stderr is the pipe). Such
 *  a process must have both elsewhere: "sleep 9 >/dev/null 2>&1 &".
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
    int err = 0;
    pid_t pid = 0;
    const char *failed = spawn_setup(&actions, &attr, fds[1], &err);
    if(!failed) {
        char *const argv[] = {"sh", "-c", (char *)command, NULL};
        err = posix_spawn(&pid, "/bin/sh", &actions, &attr, argv, environ);
        if(err != 0) {
            failed = "posix_spawn()";
        }
        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(fds[1]);
    if(failed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Command not run: a call to spawn it FAILED",
            "failed",       "%s", failed,
            "command",      "%s", command,
            "errno",        "%d", err,
            "strerror",     "%s", strerror(err),
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
        if(errno == ECHILD) {
            break;  // reaped already (SIGCHLD at SIG_IGN, or a handler that reaps): no status needed here
        }
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
 *  reaches the program as it is. The program starts as spawn_setup() says:
 *  no signal blocked, every one at its default, no fd of the caller but
 *  stdin, stdout and stderr. Up to 7.29.3 it inherited the caller's mask,
 *  ignores and every fd without O_CLOEXEC.
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
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int err = 0;
    const char *failed = spawn_setup(&actions, &attr, -1, &err);
    if(!failed) {
        err = posix_spawn(&pid, path, &actions, &attr, argv, environ);
        if(err != 0) {
            failed = "posix_spawn()";
        }
        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&actions);
    }
    if(failed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Program not run: a call to spawn it FAILED",
            "failed",       "%s", failed,
            "path",         "%s", path,
            "errno",        "%d", err,
            "strerror",     "%s", strerror(err),
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
 *  Run a command with /bin/sh -c in a pseudo terminal, and pass the
 *  caller's terminal to it and its output to the caller's, until it ends.
 *  The command starts with no signal blocked, every signal at its default
 *  and no fd of the caller but the terminal (clean_child_before_exec()).
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
        clean_child_before_exec(-1);
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
        BOOL stdin_open = TRUE;
        while(1) {
            fd_set read_fd;
            fd_set write_fd;
            fd_set except_fd;

            FD_ZERO(&read_fd);
            FD_ZERO(&write_fd);
            FD_ZERO(&except_fd);

            FD_SET(master, &read_fd);
            if(stdin_open) {
                FD_SET(STDIN_FILENO, &read_fd);
            }

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

            if (stdin_open && FD_ISSET(STDIN_FILENO, &read_fd)) {
                ssize_t x = read(STDIN_FILENO, &input, 1);  // read from tty
                if(x == 0 || (x < 0 && errno != EINTR && errno != EAGAIN)) {
                    /*
                     *  The end of the caller's stdin (or its error): nothing
                     *  more to pass on. Up to 7.29.3 it was read again at
                     *  once, for ever, and each time a byte that was never
                     *  read was written to the program.
                     */
                    stdin_open = FALSE;
                } else if(x > 0) {
                    x = write(master, &input, 1);   // write to program
                    if(x) {} // avoid warning
                }
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
